#include "RTSHUD.h"
#include "Misc/App.h"
#include "RTSHudPainter.h"

#include "RTSNetSession.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RTSCameraPawn.h"
#include "RTSFogOfWar.h"
#include "RTSGameData.h"
#include "RTSSessionSubsystem.h"
#include "TextureResource.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// O jogo é em inglês por padrão (decisão do Felipe, 09/10). Todo texto da tela passa por
// LOCTEXT, pra que o português e outras línguas entrem depois como tradução.
#define LOCTEXT_NAMESPACE "RTSHUD"

namespace
{
	const FLinearColor TextColor(0.92f, 0.93f, 0.95f, 1.0f);
	const FLinearColor DimText(0.62f, 0.66f, 0.72f, 1.0f);
	const FLinearColor Accent(0.95f, 0.62f, 0.18f, 1.0f);
	const FLinearColor ButtonFill(0.07f, 0.08f, 0.10f, 0.80f);
	const FLinearColor ButtonHover(0.22f, 0.16f, 0.08f, 0.90f);
	const FLinearColor MoveGreen(0.35f, 1.0f, 0.4f, 1.0f);
	const FLinearColor AttackRed(1.0f, 0.25f, 0.2f, 1.0f);

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
	 * A fonte da engine: a Roboto composta, com Regular e Bold de verdade. O Canvas só
	 * rasteriza no tamanho da FSlateFontInfo que recebe (CanvasItem.cpp,
	 * DrawStringInternal_RuntimeCache: GetCharacterList(FontInfo, DPIScale)), e a escala do
	 * item só estica o quadradinho de cada letra. Antes a fonte saía no tamanho legado e era
	 * esticada, com o negrito feito de duas cópias a 0,6 px: o texto borrava (09/10).
	 * Size em pontos, como no Slate (96 dpi = 1,333 px por ponto).
	 */
	FSlateFontInfo LabelFont(int32 Size, bool bBold)
	{
		return FSlateFontInfo(GEngine->GetLargeFont(), Size, bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	}

	/** Posição em pixel inteiro da tela (a letra rasterizada cai em cima dos pixels, sem filtrar). */
	float SnapPx(float V, float Dpi)
	{
		return FMath::RoundToFloat(V * Dpi) / Dpi;
	}
}

FVector2D ARTSHUD::MousePos() const
{
	float X = -1.0f;
	float Y = -1.0f;
	if (PlayerOwner)
	{
		if (const ARTSPlayerController* RTSPC = Cast<ARTSPlayerController>(PlayerOwner))
		{
			RTSPC->ReadMouse(X, Y);
		}
		else
		{
			PlayerOwner->GetMousePosition(X, Y);
		}
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

FString ARTSHUD::ButtonIdsForTest() const
{
	TArray<FString> Ids;
	for (const FButton& B : Buttons)
	{
		Ids.Add(B.Id.ToString());
	}
	return FString::Join(Ids, TEXT(" "));
}

void ARTSHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, W, H);
}

FVector2D ARTSHUD::MeasureLabel(const FString& Text, int32 Size, bool bBold) const
{
	// a mesma medida que o Canvas usa por dentro (CanvasItem.cpp, GetTextSizeInternal): sai em
	// pixels da tela, e o HUD trabalha em pixels do canvas
	const float Dpi = Canvas ? Canvas->GetDPIScale() : 1.0f;
	if (!FSlateApplication::IsInitialized())
	{
		return FVector2D::ZeroVector;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return Measure->Measure(Text, LabelFont(Size, bBold), Dpi) / Dpi;
}

FVector2D ARTSHUD::DrawLabel(const FString& Text, float X, float Y, int32 Size, const FLinearColor& Color, bool bCenter, bool bBold)
{
	const FVector2D Extent = MeasureLabel(Text, Size, bBold);
	if (!Canvas)
	{
		return Extent;
	}
	const float Dpi = Canvas->GetDPIScale();
	const float DrawX = bCenter ? X - Extent.X * 0.5f : X;
	FCanvasTextItem Item(FVector2D(SnapPx(DrawX, Dpi), SnapPx(Y, Dpi)), FText::FromString(Text), LabelFont(Size, bBold), Color);
	// sombra de pixel inteiro: 1 px no texto do jogo, 2 px nos títulos grandes
	Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f * Color.A), FVector2D(Size >= 24 ? 2.0f : 1.0f));
	Canvas->DrawItem(Item);
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

void ARTSHUD::DrawSmallButton(FName Id, const FString& Label, float X, float Y, float W, float H, const FLinearColor* Swatch)
{
	const FBox2D Box(FVector2D(X, Y), FVector2D(X + W, Y + H));
	const bool bHover = Box.IsInside(MousePos());
	DrawPanel(X, Y, W, H, bHover ? ButtonHover : ButtonFill);
	DrawRect(bHover ? Accent : FLinearColor(1, 1, 1, 0.16f), X, Y + H - 2.0f, W, 2.0f);
	if (Swatch)
	{
		DrawPanel(X + 6.0f, Y + 6.0f, W - 12.0f, H - 12.0f, *Swatch);
	}
	else
	{
		const FVector2D Extent = MeasureLabel(Label, 12, true);
		DrawLabel(Label, X + (W - Extent.X) * 0.5f, Y + (H - Extent.Y) * 0.5f, 12, bHover ? FLinearColor::White : TextColor, false, true);
	}
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
		const bool bAwaitNet = Match && Match->IsAwaitingNetSetup();
		DrawLabel(bError ? PC->GetError() : (bAwaitNet ? LOCTEXT("ReceivingMatch", "Receiving the match from the host…").ToString() : LOCTEXT("SettingUp", "Setting up the match…").ToString()),
			Canvas->SizeX * 0.5f, Canvas->SizeY * 0.45f, 15, bError ? AttackRed : TextColor, true);
		DrawButton(TEXT("menu"), LOCTEXT("ReturnToMenu", "RETURN TO MENU").ToString(), Canvas->SizeX * 0.5f - 170.0f, Canvas->SizeY * 0.55f, 340.0f, 52.0f);
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
	DrawLabel(LOCTEXT("Tagline", "prototype  ·  local skirmish").ToString(), X, Y, 12, DimText);

	const URTSNetSession* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSNetSession>() : nullptr;
	const float BW = PanelW - X * 2.0f;
	if (Net && PC->GetMenuPage() != ARTSPlayerController::EMenuPage::Main)
	{
		DrawLanMenu(PC, *Net, X, H * 0.42f, BW);
		return;
	}

	Y = H * 0.44f;
	DrawButton(TEXT("skirmish"), LOCTEXT("PlaySkirmish", "PLAY SKIRMISH").ToString(), X, Y, BW, 58.0f);
	DrawLabel(LOCTEXT("SkirmishInfo2", "Desert (salt flats)  ·  you vs AI  ·  base, economy, tanks").ToString(), X + 2.0f, Y + 68.0f, 11, DimText);
	Y += 96.0f;
	DrawButton(TEXT("multiplayer"), LOCTEXT("Multiplayer", "MULTIPLAYER (LAN)").ToString(), X, Y, BW, 58.0f);
	DrawLabel(LOCTEXT("MultiplayerInfo", "host or join a game on your local network").ToString(), X + 2.0f, Y + 68.0f, 11, DimText);
	Y += 96.0f;
	const URTSSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
	const ERTSFogMode Fog = Session ? Session->FogMode : ERTSFogMode::Explored;
	DrawButton(TEXT("fogsetting"), FogModeText(Fog), X, Y, PanelW - X * 2.0f, 44.0f);
	DrawLabel(FogModeHint(Fog), X + 2.0f, Y + 52.0f, 10, DimText);
	Y += 80.0f;
	DrawQualityRow(PC, X, Y, BW);
	Y += 80.0f;
	DrawButton(TEXT("quit"), LOCTEXT("Quit", "QUIT").ToString(), X, Y, PanelW - X * 2.0f, 58.0f);
}

FString ARTSHUD::FogModeText(ERTSFogMode Mode)
{
	return Mode == ERTSFogMode::Explored ? LOCTEXT("FogExplored", "FOG: EXPLORED MAP").ToString()
		: (Mode == ERTSFogMode::Black ? LOCTEXT("FogBlack", "FOG: BLACK").ToString() : LOCTEXT("FogOffButton", "FOG: OFF").ToString());
}

FString ARTSHUD::FogModeHint(ERTSFogMode Mode)
{
	return Mode == ERTSFogMode::Explored ? LOCTEXT("FogExploredHint", "the map is visible; enemies only where you can see").ToString()
		: (Mode == ERTSFogMode::Black ? LOCTEXT("FogBlackHint", "what you never saw stays black").ToString() : LOCTEXT("FogOffHint", "everything visible").ToString());
}

void ARTSHUD::DrawLanMenu(ARTSPlayerController* PC, const URTSNetSession& Net, float X, float Y, float BW)
{
	if (PC->GetMenuPage() == ARTSPlayerController::EMenuPage::Lan)
	{
		DrawLabel(LOCTEXT("LanTitle", "MULTIPLAYER  ·  LAN").ToString(), X, Y, 18, TextColor, false, true);
		Y += 44.0f;
		DrawButton(TEXT("lan_host"), LOCTEXT("HostGame", "HOST GAME").ToString(), X, Y, BW, 58.0f);
		Y += 84.0f;
		DrawLabel(LOCTEXT("GamesOnNetwork", "Games on your network").ToString(), X + 2.0f, Y, 12, DimText);
		Y += 26.0f;
		const TArray<FRTSLanGame>& Games = Net.FoundGames();
		if (Games.IsEmpty())
		{
			DrawLabel(LOCTEXT("Searching", "Searching...").ToString(), X + 2.0f, Y + 6.0f, 12, DimText);
			Y += 44.0f;
		}
		for (int32 K = 0; K < Games.Num() && K < 5; ++K)
		{
			const FRTSLanGame& G = Games[K];
			const FString Label = FText::Format(LOCTEXT("JoinGame", "JOIN  {0}   {1}/8"), FText::FromString(G.Name), FText::AsNumber(G.Players)).ToString();
			if (G.bOpen)
			{
				DrawButton(FName(*FString::Printf(TEXT("lan_join_%d"), K)), Label, X, Y, BW, 46.0f);
			}
			else
			{
				DrawLabel(Label + TEXT("   ") + LOCTEXT("InProgress", "(in progress or other version)").ToString(), X + 2.0f, Y + 12.0f, 12, DimText);
			}
			Y += 56.0f;
		}
		// a sala não aparece (firewall, rede virtual no anfitrião): entra direto pelo IP dele
		Y += 10.0f;
		DrawLabel(LOCTEXT("JoinByIpTitle", "Not in the list? Type the host's IP").ToString(), X + 2.0f, Y, 12, DimText);
		Y += 24.0f;
		const bool bTyping = PC->IsTypingIp();
		const bool bCaret = bTyping && FMath::Fmod(float(FPlatformTime::Seconds()), 1.0f) < 0.5f;
		const FString IpText = PC->GetIpEntry().IsEmpty() && !bTyping ? LOCTEXT("IpHint", "192.168.1.___").ToString() : PC->GetIpEntry() + (bCaret ? TEXT("|") : TEXT(" "));
		DrawSmallButton(TEXT("lan_ip"), FString(), X, Y, BW - 96.0f, 40.0f);
		DrawLabel(IpText, X + 14.0f, Y + 10.0f, 15, bTyping || !PC->GetIpEntry().IsEmpty() ? TextColor : DimText, false, true);
		DrawSmallButton(TEXT("lan_ip_join"), LOCTEXT("JoinIp", "JOIN").ToString(), X + BW - 88.0f, Y, 88.0f, 40.0f);
		Y += 62.0f;
		DrawButton(TEXT("lan_back"), LOCTEXT("Back", "BACK").ToString(), X, Y, BW, 50.0f);
		return;
	}
	// a sala de espera
	DrawLabel(LOCTEXT("LobbyTitle", "LOBBY  ·  LAN").ToString(), X, Y, 18, TextColor, false, true);
	Y += 40.0f;
	DrawLabel(LOCTEXT("LobbyMap", "Desert (salt flats)  ·  base, economy, tanks").ToString(), X + 2.0f, Y, 12, DimText);
	Y += 34.0f;
	const bool bHost = Net.IsHost();
	// as colunas da sala (como a do ZH): jogador · time · cor · (tirar a IA)
	const float TeamX = X + BW - 156.0f;
	const float ColorX = X + BW - 96.0f;
	const float KickX = X + BW - 34.0f;
	DrawLabel(LOCTEXT("ColPlayer", "PLAYER").ToString(), X + 16.0f, Y, 10, DimText);
	DrawLabel(LOCTEXT("ColTeam", "TEAM").ToString(), TeamX + 12.0f, Y, 10, DimText);
	DrawLabel(LOCTEXT("ColColor", "COLOR").ToString(), ColorX + 8.0f, Y, 10, DimText);
	Y += 18.0f;
	for (const FRTSNetMember& M : Net.Members())
	{
		const FLinearColor C = URTSMatchSubsystem::PaletteColor(M.Color);
		const bool bMe = M.Slot == Net.LocalSlot();
		const float RowH = 40.0f;
		DrawPanel(X, Y, BW, RowH, FLinearColor(0.03f, 0.035f, 0.045f, 0.85f));
		DrawPanel(X, Y, 6.0f, RowH, C);
		// o nome em cima, as marcas embaixo (o nome do PC pode ser comprido)
		DrawLabel(M.bAI ? LOCTEXT("AiName", "Computer").ToString() : M.Name.Left(16), X + 16.0f, Y + 4.0f, 14, M.bInLobby ? TextColor : DimText);
		FString RowTags;
		RowTags += M.Slot == 0 ? LOCTEXT("HostTag", "host").ToString() + TEXT("   ") : FString();
		RowTags += bMe ? LOCTEXT("YouTag", "you").ToString() + TEXT("   ") : FString();
		RowTags += M.bAI ? LOCTEXT("AiTag", "AI").ToString() + TEXT("   ") : FString();
		RowTags += !M.bInLobby ? LOCTEXT("InMatchTag", "still on the results").ToString() : FString();
		DrawLabel(RowTags, X + 16.0f, Y + 23.0f, 10, DimText);
		// o time: o seu (qualquer um) ou o de qualquer vaga (o anfitrião); o resto só mostra
		const FString TeamText = M.Team == 0 ? TEXT("-") : FString::Printf(TEXT("%d"), M.Team);
		if (bMe || bHost)
		{
			DrawSmallButton(FName(*FString::Printf(TEXT("lan_team_%d"), M.Slot)), TeamText, TeamX, Y + 6.0f, 52.0f, 28.0f);
		}
		else
		{
			const FVector2D Extent = MeasureLabel(TeamText, 13, true);
			DrawLabel(TeamText, TeamX + (52.0f - Extent.X) * 0.5f, Y + 12.0f, 13, TextColor, false, true);
		}
		// a cor: o próprio quadrado é o botão (a sua, ou a da IA pro anfitrião)
		if (bMe || (bHost && M.bAI))
		{
			DrawSmallButton(FName(*FString::Printf(TEXT("lan_color_%d"), M.Slot)), FString(), ColorX, Y + 6.0f, 52.0f, 28.0f, &C);
		}
		else
		{
			DrawPanel(ColorX + 6.0f, Y + 12.0f, 40.0f, 16.0f, C);
		}
		if (bHost && M.bAI)
		{
			DrawSmallButton(FName(*FString::Printf(TEXT("lan_kick_%d"), M.Slot)), TEXT("X"), KickX, Y + 6.0f, 28.0f, 28.0f);
		}
		Y += RowH + 4.0f;
	}
	Y += 6.0f;
	// a névoa (o anfitrião escolhe; os outros vêem), a IA e o sorteio dos times
	if (bHost)
	{
		DrawSmallButton(TEXT("lan_fog"), FogModeText(Net.GetFogMode()), X, Y, 164.0f, 34.0f);
		if (Net.PlayerCount() < Net.SlotLimit())
		{
			DrawSmallButton(TEXT("lan_add_ai"), LOCTEXT("AddAi", "+ AI").ToString(), X + 170.0f, Y, 60.0f, 34.0f);
		}
		DrawSmallButton(TEXT("lan_random"), LOCTEXT("RandomTeams", "RANDOM TEAMS").ToString(), X + BW - 116.0f, Y, 116.0f, 34.0f);
		Y += 44.0f;
	}
	else
	{
		DrawLabel(FogModeText(Net.GetFogMode()), X + 2.0f, Y + 4.0f, 12, DimText);
		Y += 30.0f;
	}
	FString Status = Net.StatusText();
	if (!Net.EveryoneInLobby())
	{
		Status = LOCTEXT("WaitingResults", "Waiting for everyone to come back to the lobby...").ToString();
	}
	else if (bHost && Net.PlayerCount() >= 2 && !Net.CanStart())
	{
		Status = LOCTEXT("NeedTwoSides", "Everyone is on the same team: make at least 2 sides").ToString();
	}
	DrawLabel(Status, X + 2.0f, Y, 12, DimText);
	Y += 30.0f;
	if (bHost && Net.CanStart())
	{
		DrawButton(TEXT("lan_start"), LOCTEXT("StartGame", "START").ToString(), X, Y, BW, 58.0f);
		Y += 72.0f;
	}
	DrawButton(TEXT("lan_leave"), LOCTEXT("Leave", "LEAVE").ToString(), X, Y, BW, 50.0f);
}

void ARTSHUD::DrawNetStatus(URTSMatchSubsystem* Match)
{
	const URTSNetSession* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSNetSession>() : nullptr;
	if (!Match->IsNetMatch() || !Net)
	{
		return;
	}
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	if (Net->IsDesynced())
	{
		// as simulações divergiram: o defeito mais grave da rede, na cara (o log tem o passo)
		DrawPanel(W * 0.5f - 300.0f, H * 0.16f, 600.0f, 46.0f, FLinearColor(0.35f, 0.02f, 0.02f, 0.9f));
		DrawLabel(FText::Format(LOCTEXT("Desync", "DESYNC at step {0}: the games diverged"), FText::AsNumber(Net->GetDesyncTick())).ToString(),
			W * 0.5f, H * 0.16f + 13.0f, 15, TextColor, true);
	}
	else if (Net->IsPeerLost())
	{
		DrawPanel(W * 0.5f - 240.0f, H * 0.16f, 480.0f, 46.0f, FLinearColor(0.25f, 0.12f, 0.02f, 0.9f));
		DrawLabel(LOCTEXT("PeerLost", "Connection lost").ToString(), W * 0.5f, H * 0.16f + 13.0f, 15, TextColor, true);
	}
	else if (Net->HostSilentSeconds() > 0.0f)
	{
		// o anfitrião é o relógio: sem notícia dele, a partida espera (até 60 s, depois "Connection lost")
		DrawPanel(W * 0.5f - 280.0f, H * 0.26f, 560.0f, 92.0f, FLinearColor(0.02f, 0.03f, 0.05f, 0.88f));
		DrawLabel(LOCTEXT("NetWaitingHost", "Waiting for the host").ToString(), W * 0.5f, H * 0.26f + 14.0f, 24, TextColor, true, true);
		DrawLabel(FText::Format(LOCTEXT("NetWaitingHostLine", "not responding for {0} s"), FText::AsNumber(FMath::FloorToInt(Net->HostSilentSeconds()))).ToString(),
			W * 0.5f, H * 0.26f + 56.0f, 12, DimText, true);
	}
	else if (Net->IsNetPaused())
	{
		// a pausa em rede (13/10): quem pausou e o botão de voltar; ou a espera por quem sumiu, com o voto
		const URTSNetSession::FPauseState& P = Net->GetPause();
		const float PW = 560.0f;
		const float PX = W * 0.5f - PW * 0.5f;
		const float PY = H * 0.26f;
		DrawPanel(PX, PY, PW, 150.0f, FLinearColor(0.02f, 0.03f, 0.05f, 0.88f));
		if (P.WaitingFor != 255)
		{
			const FText Who = URTSMatchSubsystem::PlayerColorName(P.WaitingFor);
			DrawLabel(FText::Format(LOCTEXT("NetWaiting", "Waiting for {0}"), Who).ToString(), W * 0.5f, PY + 16.0f, 24, TextColor, true, true);
			DrawLabel(FText::Format(LOCTEXT("NetWaitingLine", "not responding  ·  dropped in {0} s  ·  votes {1}/{2}"),
				FText::AsNumber(FMath::CeilToInt(P.WaitLeft)), FText::AsNumber(FMath::CountBits(P.Votes)), FText::AsNumber(P.VotesNeeded)).ToString(),
				W * 0.5f, PY + 56.0f, 12, DimText, true);
			if (Net->LocalSlot() != P.WaitingFor && !(P.Votes & (1u << Net->LocalSlot())))
			{
				DrawButton(TEXT("net_vote"), FText::Format(LOCTEXT("NetVote", "DROP {0}"), Who).ToString(), W * 0.5f - 150.0f, PY + 86.0f, 300.0f, 46.0f);
			}
		}
		else if (P.ResumeIn >= 0.0f)
		{
			DrawLabel(FText::Format(LOCTEXT("NetResuming", "Resuming in {0}"), FText::AsNumber(FMath::CeilToInt(P.ResumeIn))).ToString(), W * 0.5f, PY + 52.0f, 28, TextColor, true, true);
		}
		else
		{
			const FText Who = P.BySlot == Net->LocalSlot() ? LOCTEXT("NetYou", "you") : URTSMatchSubsystem::PlayerColorName(P.BySlot);
			DrawLabel(FText::Format(LOCTEXT("NetPausedBy", "PAUSED by {0}"), Who).ToString(), W * 0.5f, PY + 16.0f, 24, TextColor, true, true);
			DrawLabel(LOCTEXT("NetPausedLine", "the game is stopped for everyone").ToString(), W * 0.5f, PY + 56.0f, 12, DimText, true);
			DrawButton(TEXT("net_resume"), LOCTEXT("NetResume", "RESUME").ToString(), W * 0.5f - 150.0f, PY + 86.0f, 300.0f, 46.0f);
		}
	}
	else if (Match->IsWaitingForPlayers())
	{
		DrawPanel(W * 0.5f - 240.0f, H * 0.16f, 480.0f, 46.0f, FLinearColor(0.02f, 0.03f, 0.05f, 0.85f));
		DrawLabel(LOCTEXT("WaitingOthers", "Waiting for the other players...").ToString(), W * 0.5f, H * 0.16f + 13.0f, 15, TextColor, true);
	}
	if (Net->IsHost())
	{
		DrawLabel(FText::Format(LOCTEXT("LanPing", "LAN  ·  ping {0} ms"), FText::AsNumber(Net->WorstRttMs())).ToString(), W - 170.0f, 48.0f, 11, DimText);
	}
	else
	{
		DrawLabel(LOCTEXT("LanClient", "LAN").ToString(), W - 170.0f, 48.0f, 11, DimText);
	}
}

void ARTSHUD::DrawMatch(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	if (!Match->IsReady())
	{
		// a tela de carregar continua (a mesma da troca de mapa) até a cena estar pronta
		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.012f, 0.014f, 0.017f, 1.0f));
		DrawLabel(LOCTEXT("Loading", "LOADING").ToString(), Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f - 30.0f, 26, FLinearColor(0.92f, 0.93f, 0.95f), true, true);
		const int32 Dots = int32(FPlatformTime::Seconds() * 3.0) % 4;
		DrawLabel(FString::ChrN(Dots, TEXT('.')), Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f + 12.0f, 20, FLinearColor(0.92f, 0.93f, 0.95f, 0.7f), true);
		return;
	}
	if (!bNewHudChecked)
	{
		bNewHud = !FParse::Param(FCommandLine::Get(), TEXT("OldHud"));   // (14/10: o novo é o padrão; F4 troca)
		bNewHudChecked = true;
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
	if (PC->IsFacingDragging(A, B))
	{
		const FLinearColor Color = PC->IsAttackMoveArmed() ? AttackRed : MoveGreen;
		const FVector2D D = (B - A).GetSafeNormal();
		const FVector2D Side(-D.Y, D.X);
		const float Len = float((B - A).Size());
		// a barra: a linha de frente no ponto apertado, da largura do arraste (arraste curto: uma barra curta)
		const float HalfBar = FMath::Max(Len * 0.5f, 18.0f);
		DrawLine(A.X - Side.X * HalfBar, A.Y - Side.Y * HalfBar, A.X + Side.X * HalfBar, A.Y + Side.Y * HalfBar, Color, 3.0f);
		// a seta: pra onde ela olha
		DrawLine(A.X, A.Y, B.X, B.Y, WithAlpha(Color, 0.8f), 2.0f);
		const FVector2D Tip = B;
		const FVector2D L = Tip - D * 14.0f + Side * 8.0f;
		const FVector2D R = Tip - D * 14.0f - Side * 8.0f;
		DrawLine(Tip.X, Tip.Y, L.X, L.Y, Color, 2.0f);
		DrawLine(Tip.X, Tip.Y, R.X, R.Y, Color, 2.0f);
	}
	if (PC->IsAttackMoveArmed())
	{
		const FVector2D M = MousePos();
		DrawLabel(LOCTEXT("AttackMoveArmed", "ATTACK MOVE").ToString(), M.X + 20.0f, M.Y + 16.0f, 11, AttackRed, false, true);
	}

	const bool bEconomy = HasEconomy(Match);
	if (bEconomy)
	{
		DrawRallyPoints(PC, Match);
		DrawPlacementGhost(PC, Match);
		DrawDockHover(PC, Match);
		DrawMoneyPopups(Match, GetWorld()->GetDeltaSeconds());
	}
	if (bNewHud)
	{
		// o HUD novo do HUDLab (RTSHUDNew.cpp): o topo, o comandante, o minimapa, os grupos, o painel e o cartão
		DrawNewHud(PC, Match);
		if (bEconomy && Match->IsLocalDefeated())
		{
			DrawObserverPanel(Match);
		}
	}
	else
	{
		if (bEconomy)
		{
			DrawEconomyTopBar(Match);
			if (Match->IsLocalDefeated())
			{
				DrawObserverPanel(Match);
			}
			else
			{
				DrawCommandPanel(PC, Match);
			}
		}
		else
		{
			DrawTopBar(Match);
		}
		DrawMinimap(PC, Match);
	}
	DrawDefeats(Match);
	const bool bPlaying = Match->Result() == ERTSMatchResult::Playing;
	if (bPlaying && Match->SecondsSinceStart() < ObjectiveSeconds && !Match->GetScenario())
	{
		DrawObjective(Match);   // no teste rápido (cenário) não há objetivo: a cena é só pra olhar
	}
	if (bPlaying && Match->MatchSeconds() < HelpSeconds && PC->OrdersGiven() < 2 && !bNewHud)   // (no HUD novo a linha caía em cima das abas dos grupos)
	{
		DrawLabel(LOCTEXT("HelpLine", "Left: select and order   ·   Right: deselect / drag to scroll   ·   A: attack move   ·   S: stop   ·   Ctrl+number: group   ·   Esc: pause").ToString(),
			Canvas->SizeX * 0.5f, Canvas->SizeY - 34.0f, 10, WithAlpha(DimText, 0.85f), true);
	}
	if (PC->ShowDebug())
	{
		DrawLabel(Match->DebugLine(), 16.0f, 46.0f, 10, Accent);
	}

	DrawNetStatus(Match);
	{
		// o FPS no canto de baixo à direita, pequeno (o desenvolvimento; o quadro real, suavizado em ~0,5 s)
		const float Ms = float(FApp::GetDeltaTime() * 1000.0);
		FrameMsSmooth += (Ms - FrameMsSmooth) * FMath::Clamp(Ms / 500.0f, 0.02f, 1.0f);
		const float Fps = 1000.0f / FMath::Max(FrameMsSmooth, 0.1f);
		const FLinearColor C = Fps >= 55.0f ? FLinearColor(0.55f, 0.85f, 0.55f, 0.75f) : (Fps >= 30.0f ? FLinearColor(0.9f, 0.8f, 0.45f, 0.75f) : FLinearColor(0.95f, 0.45f, 0.4f, 0.8f));
		DrawLabel(FString::Printf(TEXT("%.0f fps  %.1f ms"), Fps, FrameMsSmooth), Canvas->SizeX - 112.0f, Canvas->SizeY - 18.0f, 9, C);
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
			continue;   // na névoa: sem barra
		}
		const FVector Ground(U.PosCm.X, U.PosCm.Y, (Map ? Map->GroundZCm(U.PosCm) : 0.0f) + 25.0f);
		const bool bSelected = Selected.Contains(U.Id);
		const bool bHover = PC->GetHoverId() == U.Id;
		const FLinearColor Team = URTSMatchSubsystem::PlayerColor(U.Owner);
		// sem anel no chão: o ZH não tem nenhum (Drawable::onSelected só pisca a unidade,
		// flashAsSelected). A seleção e o mouse por cima aparecem pela barra de vida
		// (Drawable::drawHealthBar). Os anéis saíram a pedido do Felipe (09/10).

		const float Ratio = float(U.Hp) / float(U.MaxHp);
		// a obra própria mostra o progresso o tempo todo (o ZH escreve "% em construção")
		const bool bBuilding = U.bStructure && U.BuildFraction < 1.0f && U.Owner == Match->LocalPlayer();
		if (!bSelected && !bHover && Ratio >= 1.0f && !bBuilding)
		{
			continue;
		}
		// prédio: a barra mais alta (acima do telhado) e mais larga
		FVector2D Top;
		if (!PlayerOwner->ProjectWorldLocationToScreen(Ground + FVector(0, 0, U.bStructure ? 1300.0f : 430.0f), Top))
		{
			continue;
		}
		const float BarW = U.bStructure ? 110.0f : 54.0f;
		const float BarH = U.bStructure ? 7.0f : 5.0f;
		const float X = Top.X - BarW * 0.5f;
		const float Y = Top.Y - 12.0f;
		DrawPanel(X - 1.0f, Y - 1.0f, BarW + 2.0f, BarH + 2.0f, WithAlpha(Team, 0.95f));
		DrawPanel(X, Y, BarW, BarH, FLinearColor(0, 0, 0, 0.75f));
		DrawPanel(X, Y, BarW * FMath::Clamp(Ratio, 0.0f, 1.0f), BarH, HealthColor(Ratio));
		if (bBuilding)
		{
			DrawPanel(X, Y + BarH + 3.0f, BarW, 4.0f, FLinearColor(0, 0, 0, 0.75f));
			DrawPanel(X, Y + BarH + 3.0f, BarW * U.BuildFraction, 4.0f, FLinearColor(0.98f, 0.80f, 0.20f));
		}
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
	DrawLabel(FText::Format(LOCTEXT("YourTanks", "Your Tanks  {0}"), Mine).ToString(), W * 0.5f - 220.0f, 9.0f, 11, Brighten(URTSMatchSubsystem::PlayerColor(Match->LocalPlayer()), 0.35f), false, true);
	const FString Right = Match->IsFogShown() ? FText::Format(LOCTEXT("Destroyed", "Destroyed  {0}"), Match->UnitsKilled()).ToString() : FText::Format(LOCTEXT("EnemyCount", "Enemy  {0}"), Theirs).ToString();
	DrawLabel(Right, W * 0.5f + 120.0f, 9.0f, 11, Brighten(URTSMatchSubsystem::PlayerColor(Match->LocalPlayer() == 0 ? 1 : 0), 0.35f), false, true);
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
	const float TitleY = Y + DrawLabel(LOCTEXT("Objective", "OBJECTIVE").ToString(), W * 0.5f, Y, 11, WithAlpha(Accent, Alpha), true, true).Y + 2.0f;
	const float SubY = TitleY + DrawLabel((HasEconomy(Match) ? LOCTEXT("ObjectiveBase", "Destroy the enemy base") : LOCTEXT("ObjectiveText", "Destroy the 6 enemy tanks")).ToString(), W * 0.5f, TitleY, 20, WithAlpha(TextColor, Alpha), true, true).Y + 6.0f;
	DrawLabel((HasEconomy(Match) ? LOCTEXT("ObjectiveBaseHint", "Select the Dozer and build: Power Plant, Supply Center, War Factory") : LOCTEXT("ObjectiveHint", "they're coming from there — follow the red arrow")).ToString(), W * 0.5f, SubY, 11, WithAlpha(DimText, Alpha), true);
}

void ARTSHUD::DrawPause()
{
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	DrawPanel(0, 0, W, H, FLinearColor(0, 0, 0, 0.55f));
	DrawLabel(LOCTEXT("Paused", "PAUSED").ToString(), W * 0.5f, H * 0.32f, 36, TextColor, true, true);
	const URTSMatchSubsystem* Match = GetWorld()->GetSubsystem<URTSMatchSubsystem>();
	const ARTSPlayerController* PC = Cast<ARTSPlayerController>(GetOwningPlayerController());
	float Y = H * 0.42f;
	DrawButton(TEXT("resume"), LOCTEXT("Resume", "RESUME").ToString(), W * 0.5f - 170.0f, Y, 340.0f, 54.0f);
	Y += 70.0f;
	if (Match && Match->IsNetMatch() && !Match->IsNetPaused())
	{
		// na rede o menu não para a partida: a pausa é pra todos (13/10)
		DrawButton(TEXT("net_pause"), LOCTEXT("NetPauseGame", "PAUSE GAME (everyone)").ToString(), W * 0.5f - 170.0f, Y, 340.0f, 54.0f);
		Y += 70.0f;
	}
	if (Match && Match->Result() == ERTSMatchResult::Playing && !Match->IsLocalDefeated())
	{
		const bool bArmed = PC && PC->IsSurrenderArmed();
		DrawButton(TEXT("surrender"), bArmed ? LOCTEXT("SurrenderConfirm", "CLICK AGAIN TO SURRENDER").ToString() : LOCTEXT("Surrender", "SURRENDER").ToString(),
			W * 0.5f - 170.0f, Y, 340.0f, 54.0f);
		Y += 70.0f;
	}
	DrawButton(TEXT("menu"), LOCTEXT("QuitToMenu", "QUIT TO MENU").ToString(), W * 0.5f - 170.0f, Y, 340.0f, 54.0f);
	Y += 70.0f;
	const bool bFog = Match && Match->IsFogOn();
	DrawButton(TEXT("fog"), bFog ? LOCTEXT("FogOn", "FOG: ON  (F9)").ToString() : LOCTEXT("FogOff", "FOG: OFF  (F9)").ToString(), W * 0.5f - 170.0f, Y, 340.0f, 44.0f);
	Y += 64.0f;
	DrawQualityRow(PC, W * 0.5f - 170.0f, Y, 340.0f);
}

void ARTSHUD::DrawQualityRow(const ARTSPlayerController* PC, float X, float Y, float W)
{
	// os 4 presets lado a lado, o de agora aceso (14/10, o Felipe: "configurar por botão também, além do F5")
	DrawLabel(LOCTEXT("QualityTitle", "QUALITY   (F5 - F8)").ToString(), X + 2.0f, Y, 10, DimText);
	const FText Names[4] = { LOCTEXT("QualityLow", "LOW"), LOCTEXT("QualityMedium", "MEDIUM"), LOCTEXT("QualityHigh", "HIGH"), LOCTEXT("QualityUltra", "ULTRA") };
	const int32 Current = PC ? PC->GetQualityPreset() : -1;
	const float Gap = 6.0f;
	const float BW = (W - Gap * 3.0f) / 4.0f;
	for (int32 Q = 0; Q < 4; ++Q)
	{
		const float BX = X + Q * (BW + Gap);
		DrawSmallButton(FName(*FString::Printf(TEXT("quality_%d"), Q)), Names[Q].ToString(), BX, Y + 18.0f, BW, 36.0f);
		if (Q == Current)
		{
			DrawRect(Accent, BX, Y + 18.0f, BW, 2.0f);
			DrawRect(Accent, BX, Y + 18.0f + 36.0f - 3.0f, BW, 3.0f);
		}
	}
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
	const FString Title = R == ERTSMatchResult::Victory ? LOCTEXT("Victory", "VICTORY").ToString() : (R == ERTSMatchResult::Defeat ? LOCTEXT("Defeat", "DEFEAT").ToString() : LOCTEXT("Draw", "DRAW").ToString());
	const FLinearColor Color = R == ERTSMatchResult::Victory ? FLinearColor(0.45f, 0.95f, 0.5f) : (R == ERTSMatchResult::Defeat ? AttackRed : TextColor);
	DrawLabel(Title, W * 0.5f, Top + 18.0f, 44, WithAlpha(Color, Alpha), true, true);
	DrawLabel(FText::Format(LOCTEXT("ResultLine", "{0} match time   ·   {1} destroyed   ·   {2} lost   ·   replay saved"),
		FText::FromString(Clock(Match->MatchSeconds())), Match->UnitsKilled(), Match->UnitsLost()).ToString(), W * 0.5f, Top + 92.0f, 12, WithAlpha(DimText, Alpha), true);
	if (Match->IsNetMatch())
	{
		// a partida em rede: todos voltam pra mesma sala (o anfitrião leva os outros junto)
		DrawButton(TEXT("lobby"), LOCTEXT("ReturnToLobby", "RETURN TO LOBBY").ToString(), W * 0.5f - 170.0f, Top + H * 0.24f + 18.0f, 340.0f, 54.0f);
		return;
	}
	DrawButton(TEXT("menu"), LOCTEXT("ReturnToMenu", "RETURN TO MENU").ToString(), W * 0.5f - 170.0f, Top + H * 0.24f + 18.0f, 340.0f, 54.0f);
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
	float F = FMath::RoundToFloat(Canvas->SizeY * MinimapFrameFraction);
	float X0 = MinimapMarginPx;
	float Y0 = Canvas->SizeY - MinimapMarginPx - F;
	MinimapFrame = FBox2D(FVector2D(X0, Y0), FVector2D(X0 + F, Y0 + F));
	MinimapCenter = FVector2D(X0 + F * 0.5f, Y0 + F * 0.5f);
	MinimapSide = F / UE_SQRT_2 * 0.97f;
	if (bNewMinimapGeometry)
	{
		// o losango da peça nova: o centro do buraco e a meia diagonal, pela conta do próprio plugin (13/10: a escala
		// é min(altura/1080, largura/1920) e, mais largo que 16:9, o HUD fica numa faixa no meio; a conta antiga saía
		// fora do buraco em 16:10 e no ultrawide); o clique só no losango (a coluna dos botões é dos botões)
		FVector2D HoleCenter;
		float Half = 0.0f;
		FRTSHudPainter::MinimapHole(Canvas->ClipX, Canvas->ClipY, HoleCenter, Half);
		MinimapCenter = HoleCenter;
		MinimapSide = Half * UE_SQRT_2;
		MinimapFrame = FBox2D(MinimapCenter - FVector2D(Half, Half), MinimapCenter + FVector2D(Half, Half));
		F = Half * 2.0f;
		X0 = MinimapCenter.X - Half;
		Y0 = MinimapCenter.Y - Half;
	}
	const ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(PC->GetPawn());
	const float YawRad = FMath::DegreesToRadians(Cam ? Cam->DefaultYawDeg : 45.0f);
	MinimapCos = FMath::Cos(YawRad);
	MinimapSin = FMath::Sin(YawRad);
	MinimapMinCm = Fog->MapMinCm();
	MinimapSizeCm = Fog->MapSizeCm();
	bMinimapDrawn = true;

	// moldura e o mapa (dois triângulos com os cantos girados: o mesmo cálculo dos pontos)
	if (!bNewMinimapGeometry)
	{
		DrawPanel(X0, Y0, F, F, FLinearColor(0.012f, 0.014f, 0.017f, 0.82f));
	}
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
	// os depósitos de suprimento: estáticos e de todos, então sempre à vista (como a prévia do mapa
	// do ZH marca os SUPPLY_SOURCE); a pilha que caiu só onde se vê
	for (int32 K = 0; K < Match->DockCount(); ++K)
	{
		FRTSDockState D;
		if (!Match->GetDock(K, D) || D.Boxes <= 0 || (D.bPile && !Match->IsPointShown(D.PosCm)))
		{
			continue;
		}
		FVector2D P;
		WorldToMinimapScreen(D.PosCm, P);
		const float S = D.bPile ? Dot * 0.8f : Dot * 1.4f;
		DrawPanel(P.X - S * 0.5f - 1.0f, P.Y - S * 0.5f - 1.0f, S + 2.0f, S + 2.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		DrawPanel(P.X - S * 0.5f, P.Y - S * 0.5f, S, S, FLinearColor(0.98f, 0.82f, 0.30f, 0.95f));
	}
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
		// prédio: um quadrado maior, do tamanho dele no mapa (e nunca menor que 1,7 ponto)
		float S = Dot;
		if (U.bStructure)
		{
			FRTSDefInfo D;
			const float SizeM = Match->GetDef(U.Def, D) ? float(FMath::Max(D.SizeM.X, D.SizeM.Y)) : 30.0f;
			S = FMath::Max(Dot * 1.7f, SizeM * 100.0f / float(FMath::Max(MinimapSizeCm.X, 1.0)) * MinimapSide);
		}
		// contorno: claro no selecionado, escuro no resto (lê em cima da areia e da salina)
		const FLinearColor Outline = Selected.Contains(U.Id) ? FLinearColor(0.95f, 0.97f, 1.0f, 0.95f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.8f);
		DrawPanel(P.X - S * 0.5f - 1.0f, P.Y - S * 0.5f - 1.0f, S + 2.0f, S + 2.0f, Outline);
		DrawPanel(P.X - S * 0.5f, P.Y - S * 0.5f, S, S, Brighten(URTSMatchSubsystem::PlayerColor(U.Owner), U.bStructure ? 0.0f : 0.15f));
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
	if (PC->AttackMessageAlpha() > 0.0f && !bNewMinimapGeometry)
	{
		DrawLabel(LOCTEXT("UnderAttack", "Unit under attack  (Space to go there)").ToString(), X0 + 2.0f, Y0 - 22.0f, 11, WithAlpha(AttackRed, PC->AttackMessageAlpha()), false, true);
	}
}

#undef LOCTEXT_NAMESPACE
