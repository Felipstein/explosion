#include "RTSHUD.h"
#include "RTSShellGame.h"   // (01/11) as telas novas (plugin RTSShell)
#include "Misc/App.h"
#include "RTSHudPainter.h"
#include "RTSHudKit.h"

#include "RTSNetSession.h"
#include "RTSChat.h"
#include "HAL/PlatformTime.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RTSCameraPawn.h"
#include "RTSFogOfWar.h"
#include "RTSGameData.h"
#include "RTSLab.h"
#include "RTSSessionSubsystem.h"
#include "TextureResource.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"
#include "RTSAlerts.h"
#include "RTSOrderMarker.h"
#include "RTSPowerPreview.h"
#include "RTSSelectFlash.h"
#include "RTSSoftDraw.h"
#include "RTSUnitActor.h"
#include "Components/StaticMeshComponent.h"
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

void ARTSHUD::DrawSeats(float X, float Y, float W, int32 Slots, int32 Filled, const FLinearColor& Team)
{
	// (01/11, a 3ª: os quadradinhos e os bonequinhos em triângulo reprovados, "horrível, pixelado, sem qualidade, sem criatividade") as
	// lâmpadas da tripulação: a cápsula de aço rebitado do kit do HUD presa embaixo da barra de vida, uma lâmpada por vaga; acesa (o azul,
	// com o capacete gravado) quem está dentro, o vidro escuro a vaga livre. As peças são textura (a arte desenhada, a borda suave),
	// recortadas da direção A do Atelier (captures/concept/72_lotacao_do_transporte_3_direcoes): skin/seat_cap_l, seat_mid, seat_cap_r,
	// seat_on e seat_off. A cápsula estica pelo número de vagas (4 no Stryker, 8 no Chinook)
	if (!Canvas || Slots <= 0)
	{
		return;
	}
	if (!NewKit)
	{
		const FString Repo = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
		NewKit = MakeShared<FRTSHudKit>(Repo / TEXT("docs/concept/hud/kit"), Repo / TEXT("HUDLab/LabAssets"));
	}
	UTexture2D* CapL = NewKit->Get(TEXT("skin:seat_cap_l"));
	UTexture2D* CapR = NewKit->Get(TEXT("skin:seat_cap_r"));
	UTexture2D* Mid = NewKit->Get(TEXT("skin:seat_mid"));
	UTexture2D* On = NewKit->Get(TEXT("skin:seat_on"));
	UTexture2D* Off = NewKit->Get(TEXT("skin:seat_off"));
	if (!CapL || !CapR || !Mid || !On || !Off)
	{
		return;
	}
	const auto Tile = [this](UTexture2D* T, float TX, float TY, float TW, float TH)
	{
		if (T && T->GetResource())
		{
			FCanvasTileItem Item(FVector2D(TX, TY), T->GetResource(), FVector2D(TW, TH), FLinearColor::White);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
	};
	// as medidas da arte (84 px de alto): as pontas de 29, a casa de cada lâmpada 77,5 e a lâmpada 62; no jogo 14 px de alto
	constexpr float ArtH = 84.0f, H = 14.0f, K = H / ArtH;
	const float CapW = 29.0f * K, Cell = 77.5f * K, Lamp = 62.0f * K;
	const float Inner = Cell * float(Slots) - 10.0f * K;
	const float Total = 2.0f * CapW + Inner;
	const float X0 = X + 0.5f * (W - Total);
	Tile(CapL, X0, Y, CapW, H);
	Tile(Mid, X0 + CapW, Y, Inner, H);
	Tile(CapR, X0 + CapW + Inner, Y, CapW, H);
	for (int32 S = 0; S < Slots; ++S)
	{
		const float Cx = X0 + CapW + (S + 0.5f) * Inner / float(Slots);
		Tile(S < Filled ? On : Off, Cx - 0.5f * Lamp, Y + 0.5f * (H - Lamp), Lamp, Lamp);
	}
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
		if (!RTSShellGame::IsCovering())   // (01/11) com as telas novas por cima, o menu antigo não se desenha
		{
			DrawMenu(PC);
		}
		return;
	}
	URTSMatchSubsystem* Match = GetWorld()->GetSubsystem<URTSMatchSubsystem>();
	if (!Match || !Match->IsRunning())
	{
		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0, 0, 0, 0.6f));
		if (RTSShellGame::IsMatchOverlay())
		{
			return;   // (06/11, Telas) o carregando e o erro de montar a partida são as telas nativas por cima
		}
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
	// (v4) com a linha do dinheiro, o menu tem 482 px do PLAY SKIRMISH ao fim do QUIT: em tela baixa (1366×768, 1280×720)
	// o título e o menu sobem juntos pra o QUIT não passar da borda (de 900 px pra cima nada muda)
	const float Lift = FMath::Max(0.0f, H * 0.44f + 482.0f + 72.0f + 16.0f - H);   // (31/10) + a linha das facções
	float Y = H * 0.20f - Lift;
	DrawLabel(TEXT("RTS"), X, Y, 54, TextColor, false, true);
	if (FRTSLab::IsAllowed() && PC->GetMenuPage() == ARTSPlayerController::EMenuPage::Main)
	{
		// (v5, 24/10) o laboratório (RTSLab.h): no jogo do desenvolvimento (o editor) e com -RTSLab; o pacote do pai fica igual
		const float LabW = 120.0f;
		const float LabX = PanelW - X - LabW;
		DrawSmallButton(TEXT("lab"), LOCTEXT("LabButton", "LAB").ToString(), LabX, Y + 16.0f, LabW, 36.0f);
		DrawLabel(LOCTEXT("LabHint", "sandbox · test live").ToString(), LabX + LabW * 0.5f, Y + 58.0f, 9, DimText, true);
	}
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

	Y = H * 0.44f - Lift;
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
	// (31/10, o elenco das 3 facções: docs/ELENCO.md) a sua facção e a da IA, lado a lado (ficam guardadas pra próxima vez)
	{
		const float Half = (BW - 12.0f) * 0.5f;
		DrawFactionPicker(TEXT("faction"), LOCTEXT("YourFaction", "YOUR FACTION").ToString(),
			Session ? Session->FactionDisplayName(Session->PlayerFaction) : TEXT("United States"), X, Y, Half);
		DrawFactionPicker(TEXT("aifaction"), LOCTEXT("EnemyFaction", "ENEMY FACTION").ToString(),
			Session ? Session->FactionDisplayName(Session->AIFaction) : TEXT("Random"), X + Half + 12.0f, Y, Half);
	}
	Y += 72.0f;
	// (v4, 22/10) o dinheiro inicial do skirmish, de 10 000 a 50 000 (fica guardado pra próxima vez)
	DrawMoneyRow(TEXT("money"), Session ? Session->StartingMoney : 10000, X, Y, BW, true);
	Y += 72.0f;
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
		// (v4) o dinheiro inicial da partida: o anfitrião escolhe, os outros vêem
		const float MoneyW = 236.0f;
		DrawMoneyRow(TEXT("lan_money"), Net.GetStartingMoney(), X, Y, MoneyW, true, true);
		const FString MoneyTitle = LOCTEXT("StartMoneyLobby", "STARTING MONEY").ToString();
		if (BW - MoneyW - 12.0f >= MeasureLabel(MoneyTitle, 10, false).X)
		{
			DrawLabel(MoneyTitle, X + MoneyW + 12.0f, Y + 11.0f, 10, DimText);
		}
		Y += 44.0f;
	}
	else
	{
		const FString FogAndMoney = FText::Format(LOCTEXT("ClientFogMoney", "{0}  ·  STARTING MONEY: {1}"),
			FText::FromString(FogModeText(Net.GetFogMode())), FText::FromString(MoneyText(Net.GetStartingMoney()))).ToString();
		DrawLabel(FogAndMoney, X + 2.0f, Y + 4.0f, 12, DimText);
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
	if (RTSShellGame::IsMatchOverlay())
	{
		return;   // (06/11, Telas) os avisos da rede nativos (longe do trecho do chat da sessão do HUD, no DrawMatch)
	}
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
		if (RTSShellGame::IsMatchOverlay())
		{
			return;   // (06/11, Telas) a tela de carregar nativa cobre
		}
		// a tela de carregar continua (a mesma da troca de mapa) até a cena estar pronta. (06/11, Telas) O "LOADING" velho saiu: a tela é a
		// nativa (o SRTSMatch); se ela ainda não estiver por cima, só o escuro dela (rgb 10, 13, 11), sem texto
		DrawPanel(0, 0, Canvas->SizeX, Canvas->SizeY, FLinearColor(0.003f, 0.004f, 0.0033f, 1.0f));
		return;
	}
	if (!bNewHudChecked)
	{
		bNewHud = !FParse::Param(FCommandLine::Get(), TEXT("OldHud"));   // (14/10: o novo é o padrão; F4 troca)
		bNewHudChecked = true;
	}
	// (16/10) quando o painel do resultado abre, as barras e os anéis do mundo saem (a barra de vida atravessava o
	// "VICTORY" pelo painel meio transparente)
	const bool bResultShown = Match->Result() != ERTSMatchResult::Playing && Match->SecondsSinceEnd() > ResultDelaySeconds;
	if (!bResultShown)
	{
		DrawUnits(PC, Match);
		DrawMarkers(PC);
		DrawOrderQueues(PC, Match);
	}

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
	if (!bResultShown)
	{
		DrawLinePreview(PC, Match);   // (v4) a linha dos dois botões: as vagas no chão
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
		DrawPowerTarget(PC, Match);    // (v4) a mira do poder do general: o círculo no chão (RTSHUDNew.cpp)
		DrawGuardPreview(PC, Match);   // (v5, 25/10) a guarda: a área no chão (RTSHUDNew.cpp)
		DrawPowerMessage(PC);
		DrawDockHover(PC, Match);
		DrawMoneyPopups(Match, GetWorld()->GetDeltaSeconds());
	}
	if (bNewHud)
	{
		// o HUD novo do HUDLab (RTSHUDNew.cpp): o topo, o comandante, o minimapa, os grupos, o painel e o cartão
		DrawNewHud(PC, Match);
		// (16/10) com o painel do resultado aberto, a lista dos jogadores sai (ele já tem o resumo, e ela caía na faixa dele)
		if (bEconomy && Match->IsLocalDefeated() && !bResultShown)
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
				if (!bResultShown)
				{
					DrawObserverPanel(Match);
				}
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
		DrawLabel(LOCTEXT("HelpLine", "Left: select and order   ·   Right: deselect / drag to scroll   ·   Left+Right drag: line up   ·   A: attack move   ·   S: stop   ·   Ctrl+number: group   ·   Esc: pause").ToString(),
			Canvas->SizeX * 0.5f, Canvas->SizeY - 34.0f, 10, WithAlpha(DimText, 0.85f), true);
	}
	if (PC->ShowDebug())
	{
		DrawLabel(Match->DebugLine(), 16.0f, 46.0f, 10, Accent);
	}

	DrawNetStatus(Match);
	DrawChat(PC, Match);
	// (04/11) a seta vermelha com o brilho da borda, por cima das peças do HUD ("tem que ficar por cima do HUD mesmo")
	RTSAlerts::DrawEdgeArrows(Canvas, PC, PC->GetMinimapPings());
	{
		// o FPS no canto de baixo à direita, pequeno (o desenvolvimento; o quadro real, suavizado em ~0,5 s)
		const float Ms = float(FApp::GetDeltaTime() * 1000.0);
		FrameMsSmooth += (Ms - FrameMsSmooth) * FMath::Clamp(Ms / 500.0f, 0.02f, 1.0f);
		const float Fps = 1000.0f / FMath::Max(FrameMsSmooth, 0.1f);
		const FLinearColor C = Fps >= 55.0f ? FLinearColor(0.55f, 0.85f, 0.55f, 0.75f) : (Fps >= 30.0f ? FLinearColor(0.9f, 0.8f, 0.45f, 0.75f) : FLinearColor(0.95f, 0.45f, 0.4f, 0.8f));
		DrawLabel(FString::Printf(TEXT("%.0f fps  %.1f ms"), Fps, FrameMsSmooth), Canvas->SizeX - 112.0f, Canvas->SizeY - 18.0f, 9, C);
	}
	if (RTSShellGame::IsMatchOverlay())
	{
		// (06/11, Telas) o resultado e o Esc nativos (o plugin RTSShell, SRTSMatch)
	}
	else if (!bPlaying && Match->SecondsSinceEnd() > ResultDelaySeconds)
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

void ARTSHUD::DrawChat(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	// (17/10) o chat da partida (RTSChat.h): as últimas linhas no canto de cima à esquerda, como no Zero Hour, com o nome na
	// cor do jogador; somem em 10 s (o último par de segundos desbotando) e voltam todas enquanto a caixa está aberta
	const URTSNetSession* Net = PC->GetGameInstance() ? PC->GetGameInstance()->GetSubsystem<URTSNetSession>() : nullptr;
	if (!Net || !Canvas)
	{
		return;
	}
	(void)Match;
	constexpr double ShowS = 10.0;
	constexpr double FadeS = 2.0;
	constexpr int32 MaxLines = 6;
	const bool bTyping = RTSChat::IsTyping();
	const double Now = FPlatformTime::Seconds();
	const float X = 18.0f;
	float Y = 86.0f;
	const TArray<URTSNetSession::FChatLine>& Lines = Net->ChatLines();
	TArray<int32, TInlineAllocator<8>> Visible;
	for (int32 K = Lines.Num() - 1; K >= 0 && Visible.Num() < MaxLines; --K)
	{
		if (!bTyping && Now - Lines[K].Time >= ShowS)
		{
			break;                          // as de antes são mais velhas ainda
		}
		Visible.Insert(K, 0);
	}
	for (const int32 K : Visible)
	{
		const URTSNetSession::FChatLine& L = Lines[K];
		const float A = bTyping ? 1.0f : float(FMath::Clamp((ShowS - (Now - L.Time)) / FadeS, 0.0, 1.0));
		FString Who;
		if (Net->IsActive())
		{
			const FRTSNetMember* M = Net->Members().FindByPredicate([&L](const FRTSNetMember& X2) { return X2.Slot == L.Slot; });
			Who = M ? M->Name : FString();
		}
		if (Who.IsEmpty())
		{
			Who = L.Slot == Net->LocalSlot() ? LOCTEXT("ChatYou", "You").ToString() : URTSMatchSubsystem::PlayerColorName(L.Slot).ToString();
		}
		const FString Head = (L.bTeam ? LOCTEXT("ChatAlliesTag", "[Allies] ").ToString() : FString()) + Who + TEXT(": ");
		const FVector2D E = DrawLabel(Head, X, Y, 11, WithAlpha(URTSMatchSubsystem::PlayerColor(L.Slot), A), false, true);
		DrawLabel(L.Text, X + E.X, Y, 11, WithAlpha(TextColor, A));
		Y += 20.0f;
	}
	if (bTyping)
	{
		const FString Prompt = RTSChat::IsTeam() ? LOCTEXT("ChatToAllies", "To allies:").ToString() : LOCTEXT("ChatToAll", "To all:").ToString();
		const bool bCursor = FMath::Fmod(Now - RTSChat::OpenedAt(), 1.0) < 0.6;
		DrawPanel(X - 8.0f, Y + 2.0f, 560.0f, 28.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		const FVector2D E = DrawLabel(Prompt + TEXT(" "), X, Y + 7.0f, 11, Accent, false, true);
		DrawLabel(RTSChat::Draft() + (bCursor ? TEXT("_") : TEXT(" ")), X + E.X, Y + 7.0f, 11, TextColor);
		DrawLabel(LOCTEXT("ChatHint", "Enter: send   ·   Tab: all / allies   ·   Esc: cancel").ToString(), X, Y + 36.0f, 9, WithAlpha(DimText, 0.85f));
	}
}

void ARTSHUD::DrawGroundRing(const FVector& Center, float RadiusCm, const FLinearColor& Color, float Thickness, int32 Segments)
{
	Segments = FMath::Max(Segments, 6);
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

void ARTSHUD::DrawOrderQueues(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	// (simulação 35, 01/11; docs/ESTUDO_TRANSPORTE_E_FILA.md §3.4) a fila da seleção, só com o Alt apertado (ele: "não deve exibir a
	// lista de tarefas ali do alt se o alt não tá mais sendo pressionado"). (03/11, o plano aprovado: docs/aprovados/hud/
	// 2026-11-01_fila_plano_no_chao_fichas_APROVADO.jpg) o mesmo passo das unidades vira uma ficha numerada no meio delas; as linhas
	// saem do pé de cada unidade até a 1ª ficha dela e depois vai uma linha só de ficha em ficha, na cor do passo de destino (verde
	// andar, vermelho atacar, âmbar o trabalho); a prévia tracejada vai da última ficha até o chão sob o mouse, onde o cursor carrega
	// a próxima ficha. Os traços lisos do RTSSoftDraw: com a linha de 2 px do Canvas cada tanque ia até a vaga dele e virava um novelo
	if (!Match->IsQueueingOrders() || !Canvas)
	{
		return;
	}
	const FRTSBakedMap* Map = Match->BakedMap();
	if (!Map)
	{
		return;
	}
	const float S = FMath::Max(0.5f, Canvas->SizeY / 900.0f);
	static const FColor KindColor[3] = { FColor(79, 224, 90), FColor(255, 59, 48), FColor(255, 178, 31) };
	const FLinearColor Casing(FColor(10, 11, 12, 150));
	// a fila que falta de cada um é o fim da mesma lista de ordens: o passo J de quem tem N passos é o passo J + (o maior N − N)
	struct FPath { FVector2D From; TArray<URTSMatchSubsystem::FRTSOrderStep> Steps; };
	TArray<FPath> Paths;
	int32 MaxSteps = 0;
	for (const uint32 Id : PC->GetSelection())
	{
		FRTSUnitState U;
		FPath P;
		if (Paths.Num() >= 40 || !Match->GetUnit(Id, U) || !Match->GetOrderPath(Id, P.Steps) || P.Steps.IsEmpty())
		{
			continue;
		}
		P.From = U.PosCm;
		MaxSteps = FMath::Max(MaxSteps, P.Steps.Num());
		Paths.Add(MoveTemp(P));
	}
	if (Paths.IsEmpty())
	{
		return;
	}
	struct FToken { FVector Sum = FVector::ZeroVector; int32 Count = 0; uint8 Kind = 0; };
	TArray<FToken> Tokens;
	Tokens.SetNum(MaxSteps);
	for (const FPath& P : Paths)
	{
		const int32 Off = MaxSteps - P.Steps.Num();
		for (int32 J = 0; J < P.Steps.Num(); ++J)
		{
			FToken& T = Tokens[Off + J];
			T.Sum += P.Steps[J].Cm;
			++T.Count;
			T.Kind = FMath::Min<uint8>(P.Steps[J].Kind, 2);
		}
	}
	const auto Center = [&](int32 K) { const FVector C = Tokens[K].Sum / float(FMath::Max(1, Tokens[K].Count)); return FVector2D(C.X, C.Y); };
	// a linha no chão (a cada 3 m), na tela
	const auto Ground = [&](const FVector2D& A, const FVector2D& B)
	{
		TArray<FVector2D> Pts;
		const int32 N = FMath::Clamp(FMath::CeilToInt32(float(FVector2D::Distance(A, B)) / 300.0f), 1, 120);
		for (int32 K = 0; K <= N; ++K)
		{
			const FVector2D P2 = FMath::Lerp(A, B, double(K) / N);
			FVector2D Px;
			if (!PlayerOwner->ProjectWorldLocationToScreen(FVector(P2.X, P2.Y, Map->GroundZCm(P2) + 30.0f), Px))
			{
				break;
			}
			Pts.Add(Px);
		}
		return Pts;
	};
	const auto Line = [&](const TArray<FVector2D>& Pts, uint8 Kind)
	{
		RTSSoftDraw::Stroke(Canvas, Pts, 4.6f * S, Casing);
		RTSSoftDraw::Stroke(Canvas, Pts, 2.6f * S, FLinearColor(KindColor[Kind]));
	};
	for (const FPath& P : Paths)
	{
		const int32 First = MaxSteps - P.Steps.Num();
		Line(Ground(P.From, Center(First)), Tokens[First].Kind);
	}
	for (int32 K = 1; K < MaxSteps; ++K)
	{
		if (Tokens[K].Count && Tokens[K - 1].Count)
		{
			Line(Ground(Center(K - 1), Center(K)), Tokens[K].Kind);
		}
	}
	// a prévia tracejada até o chão sob o mouse (o raio do mouse no relevo, em duas voltas)
	FVector Origin, Dir;
	if (PC->DeprojectMousePositionToWorld(Origin, Dir) && Dir.Z < -0.05)
	{
		const FVector2D Last = Center(MaxSteps - 1);
		float Z = Map->GroundZCm(Last);
		FVector Hit = Origin;
		for (int32 It = 0; It < 2; ++It)
		{
			Hit = Origin + Dir * ((Z - Origin.Z) / Dir.Z);
			Z = Map->GroundZCm(FVector2D(Hit.X, Hit.Y));
		}
		const TArray<FVector2D> Pts = Ground(Last, FVector2D(Hit.X, Hit.Y));
		const float On = 14.0f * S, Off = 9.0f * S;
		float Walked = 0.0f;
		TArray<FVector2D> Dash;
		for (int32 K = 1; K < Pts.Num(); ++K)
		{
			const FVector2D A = Pts[K - 1], B = Pts[K];
			const float Len = float(FVector2D::Distance(A, B));
			float T = 0.0f;
			while (T < Len)
			{
				const float Phase = FMath::Fmod(Walked + T, On + Off);
				const bool bOn = Phase < On;
				const float Step = FMath::Min(Len - T, bOn ? On - Phase : On + Off - Phase);
				if (bOn)
				{
					if (Dash.IsEmpty())
					{
						Dash.Add(FMath::Lerp(A, B, double(T / Len)));
					}
					Dash.Add(FMath::Lerp(A, B, double((T + Step) / Len)));
				}
				else if (Dash.Num() >= 2)
				{
					Line(Dash, 0);
					Dash.Reset();
				}
				T += FMath::Max(Step, 0.01f);
			}
			Walked += Len;
		}
		if (Dash.Num() >= 2)
		{
			Line(Dash, 0);
		}
	}
	// as fichas por cima das linhas: a sombrinha, o anel escuro, o disco na cor do passo e o número da vez
	for (int32 K = 0; K < MaxSteps; ++K)
	{
		if (!Tokens[K].Count)
		{
			continue;
		}
		const FVector2D C2 = Center(K);
		FVector2D P;
		if (!PlayerOwner->ProjectWorldLocationToScreen(FVector(C2.X, C2.Y, Map->GroundZCm(C2) + 30.0f), P))
		{
			continue;
		}
		RTSSoftDraw::Disc(Canvas, P + FVector2D(0.0f, 1.5f * S), 11.5f * S, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
		RTSSoftDraw::Disc(Canvas, P, 10.5f * S, FLinearColor(FColor(10, 11, 12)));
		RTSSoftDraw::Disc(Canvas, P, 9.0f * S, FLinearColor(KindColor[Tokens[K].Kind]));
		const FString Num = K < 9 ? FString::FromInt(K + 1) : FString(TEXT("+"));
		const int32 Size = FMath::RoundToInt(11.0f * S);
		const FVector2D Text = MeasureLabel(Num, Size, true);
		DrawLabel(Num, P.X, P.Y - Text.Y * 0.5f, Size, FLinearColor::White, true, true);
	}
}

void ARTSHUD::DrawHealthBar(float X, float Y, float W, float H, float S, float Ratio, float ChipFrom, float ChipFade, bool bBuild, int32 MaxHp, int32 Vet,
	const FLinearColor& Team)
{
	// (01/11, a barra A escolhida: docs/APROVADOS.md e captures/concept/barras/board.html) a cor corre contínua do verde ao vermelho (a
	// do ZH, Drawable::drawHealthBar), o contorno no mesmo tom escurecido, o brilho de cima, um risco a cada 100 de vida, o pedaço perdido
	// no tiro em branco; a obra é a própria barra, azul → ciano (o ZH). As cores em sRGB, como no quadro aprovado
	const auto Mix = [](const FColor& A, const FColor& B, float T)
	{
		return FColor(uint8(FMath::RoundToInt(FMath::Lerp(float(A.R), float(B.R), T))), uint8(FMath::RoundToInt(FMath::Lerp(float(A.G), float(B.G), T))),
			uint8(FMath::RoundToInt(FMath::Lerp(float(A.B), float(B.B), T))));
	};
	static const FColor Green(79, 214, 90), Yellow(242, 204, 58), Red(240, 74, 51), Blue(46, 125, 255), Cyan(63, 224, 240), Gold(236, 196, 92);
	const float R = FMath::Clamp(Ratio, 0.0f, 1.0f);
	const FColor Fill = bBuild ? Mix(Blue, Cyan, R) : (R >= 0.5f ? Mix(Yellow, Green, (R - 0.5f) / 0.5f) : Mix(Red, Yellow, R / 0.5f));
	const FColor Edge(uint8(Fill.R * 0.28f), uint8(Fill.G * 0.28f), uint8(Fill.B * 0.28f));
	DrawPanel(X - 1.0f, Y - 1.0f, W + 2.0f, H + 2.0f, FLinearColor(Edge).CopyWithNewOpacity(0.95f));
	DrawPanel(X, Y, W, H, FLinearColor(FColor(20, 24, 29)).CopyWithNewOpacity(0.82f));
	const float FillW = FMath::RoundToFloat(W * R);
	DrawPanel(X, Y, FillW, H, FLinearColor(Fill));
	DrawPanel(X, Y, FillW, 1.0f, FLinearColor(Mix(Fill, FColor::White, 0.35f)));
	if (!bBuild && ChipFade > 0.0f && ChipFrom > R)
	{
		const float ChipW = FMath::RoundToFloat(W * FMath::Min(ChipFrom, 1.0f)) - FillW;
		if (ChipW > 0.0f)
		{
			DrawPanel(X + FillW, Y, ChipW, H, FLinearColor(1.0f, 1.0f, 1.0f, 0.92f * ChipFade));
		}
	}
	if (!bBuild && MaxHp > 100)
	{
		for (int32 Hp = 100; Hp < MaxHp; Hp += 100)
		{
			const float TickX = X + FMath::RoundToFloat(W * float(Hp) / float(MaxHp));
			if (TickX < X + FillW)
			{
				DrawPanel(TickX, Y + 1.0f, 1.0f, H - 1.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
			}
		}
	}
	if (Vet <= 0)
	{
		// (04/11, ele: "tira esse quadrado da cor do jogador ao lado da vida") sem patente, a barra fica sozinha; a cor do time está
		// no próprio modelo e no clarão da seleção
		return;
	}
	// a insígnia da patente no lugar da marquinha, afastada 3 px da barra (ele: "não precisa deixar colada na barra"): a plaquinha escura
	// com a borda na cor do time e as divisas douradas grossas (o contraste ~12:1 na areia); 1 divisa, 2 divisas, a estrela
	const float IS = FMath::RoundToFloat(15.0f * S);
	const float PX = X - IS - FMath::RoundToFloat(5.0f * S), PY = FMath::RoundToFloat(Y + H * 0.5f - IS * 0.5f);
	DrawPanel(PX - 1.0f, PY - 1.0f, IS + 2.0f, IS + 2.0f, FLinearColor(0.002f, 0.002f, 0.003f, 0.95f));
	DrawPanel(PX, PY, IS, IS, Team);
	DrawPanel(PX + 1.0f, PY + 1.0f, IS - 2.0f, IS - 2.0f, FLinearColor(FColor(14, 17, 21)));
	const FLinearColor Gilt(Gold);
	const float CX = PX + IS * 0.5f, Thick = 2.2f * S;
	if (Vet >= 3)
	{
		// a estrela do heroico: o pentagrama de traço grosso
		const float Rad = IS * 0.36f, CY = PY + IS * 0.5f + 0.5f;
		for (int32 K = 0; K < 5; ++K)
		{
			const float A0 = -HALF_PI + K * TWO_PI / 5.0f, A1 = A0 + 2.0f * TWO_PI / 5.0f;   // de ponta em ponta, pulando uma
			DrawLine(CX + FMath::Cos(A0) * Rad, CY + FMath::Sin(A0) * Rad, CX + FMath::Cos(A1) * Rad, CY + FMath::Sin(A1) * Rad, Gilt, Thick * 0.8f);
		}
		return;
	}
	const float Half = IS * 0.30f, Rise = IS * 0.22f, Gap = IS * 0.27f;
	const float Top0 = PY + IS * 0.5f - (Vet - 1) * Gap * 0.5f - Rise * 0.5f;
	for (int32 K = 0; K < Vet; ++K)
	{
		const float YY = Top0 + K * Gap;
		DrawLine(CX - Half, YY + Rise, CX, YY, Gilt, Thick);
		DrawLine(CX, YY, CX + Half, YY + Rise, Gilt, Thick);
	}
}

void ARTSHUD::DrawUnits(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	const FRTSBakedMap* Map = Match->BakedMap();
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	const TArray<uint32>& Selected = PC->GetSelection();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float S = Canvas ? FMath::Max(0.5f, Canvas->SizeY / 900.0f) : 1.0f;   // a escala da barra: 1 em 900 linhas
	// (04/11) quem acabou de entrar na seleção ganha o clarão na cor do time e o estalo da barra (o flashAsSelected do ZH)
	TSet<uint32> NewlySelected;
	// -RTSFlashTest: a seleção "entra de novo" a cada 1,5 s (a prova em vídeo do clarão, que dura 0,28 s)
	static const bool bFlashTest = FParse::Param(FCommandLine::Get(), TEXT("RTSFlashTest"));
	if (bFlashTest && FMath::FloorToInt(Now / 1.5) != FMath::FloorToInt(LastFlashTestAt / 1.5))
	{
		LastSelectedIds.Reset();
	}
	LastFlashTestAt = Now;
	for (const uint32 Id : Selected)
	{
		if (!LastSelectedIds.Contains(Id))
		{
			NewlySelected.Add(Id);
			SelectPopAt.Add(Id, Now);
		}
	}
	LastSelectedIds = TSet<uint32>(Selected);
	if (SelectPopAt.Num() > 512)
	{
		SelectPopAt.Reset();
	}
	RTSSelectFlash::Tick(GetWorld());
	int32 SelectFlashes = 0;
	if (BarLastRatio.Num() > 4096)
	{
		BarLastRatio.Reset();   // (os mortos não saem um a um; de vez em quando, tudo)
		BarChips.Reset();
	}
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

		// (v5, 24/10) o ar: a aeronave selecionada tem o anel pequeno no chão embaixo dela (a altura lê pela sombra); a defesa e o
		// antiaéreo selecionados mostram o alcance de ar no chão, diferente do de chão (a crítica 7 do ZH, docs/AEREO.md §3.6)
		FRTSDefInfo Info;
		const bool bInfo = (bSelected || bHover) && Match->GetDef(U.Def, Info);
		if (bSelected && U.bAir && U.AltCm > 200.0f)
		{
			DrawGroundRing(Ground, 260.0f, WithAlpha(Team, 0.8f), 1.5f, 24);
		}
		if (bInfo && bSelected && Info.AirRangeM > 0.0f && U.Owner == Match->LocalPlayer() && !U.bAir)
		{
			DrawGroundRing(Ground, Info.AirRangeM * 100.0f, FLinearColor(0.35f, 0.75f, 1.0f, 0.55f), 1.5f, 72);
		}
		if (U.bAir && Match->IsLab() && U.Owner == Match->LocalPlayer())
		{
			// (v5) o laboratório: o estado de voo e a altura por cima de cada aeronave (a depuração do ar, docs/AEREO.md §3.13)
			static const TCHAR* States[] = { TEXT("-"), TEXT("parked"), TEXT("taxi out"), TEXT("runway"), TEXT("takeoff"), TEXT("flying"), TEXT("approach"),
				TEXT("landing"), TEXT("taxi in") };
			FVector2D Tag;
			if (PlayerOwner->ProjectWorldLocationToScreen(Ground + FVector(0, 0, U.AltCm + 650.0f), Tag))
			{
				const FString Line = FString::Printf(TEXT("%s  %.0f m%s%s"), U.Flight < UE_ARRAY_COUNT(States) ? States[U.Flight] : TEXT("?"), U.AltCm / 100.0f,
					U.bReturning ? TEXT("  RTB") : TEXT(""), U.bPatrol ? TEXT("  PATROL") : TEXT(""));
				DrawLabel(Line, Tag.X, Tag.Y, 10, FLinearColor(0.75f, 0.9f, 1.0f, 0.9f), true);
			}
		}
		const float Ratio = float(U.Hp) / float(U.MaxHp);
		// (01/11) o pedaço perdido no tiro: a vida de antes fica branca e some em 0,6 s (tiros seguidos somam no mesmo pedaço)
		float& LastRatio = BarLastRatio.FindOrAdd(U.Id, Ratio);
		if (Ratio < LastRatio - 0.001f)
		{
			FBarChip& Chip = BarChips.FindOrAdd(U.Id);
			Chip.From = (Chip.At >= 0.0 && Now - Chip.At < 0.6) ? FMath::Max(Chip.From, LastRatio) : LastRatio;
			Chip.At = Now;
		}
		LastRatio = Ratio;
		// a obra própria mostra o progresso o tempo todo (o ZH escreve "% em construção")
		const bool bBuilding = U.bStructure && U.BuildFraction < 1.0f && U.Owner == Match->LocalPlayer();
		if (!bSelected && !bHover && Ratio >= 1.0f && !bBuilding)
		{
			continue;
		}
		// (01/11, a barra A escolhida) encostada no topo do modelo e da largura dele na tela, como o ZH (computeHealthRegion: o raio
		// maior + o menor, acompanhando o zoom). Na unidade, o topo e a largura saem das peças de malha visíveis (o casco, a torre);
		// peça maior que 50 m não conta. No prédio, da planta e do casco (abaixo). Sem o modelo, as alturas de antes
		FVector TopWorld = Ground + FVector(0, 0, U.bStructure ? 1300.0f : 430.0f + (U.bAir ? U.AltCm : 0.0f));
		float HalfCm = U.bStructure ? 900.0f : 300.0f;
		const ARTSUnitActor* View = Match->ViewOf(U.Id);
		if (View && U.bStructure)
		{
			// o prédio: a planta e a altura do casco, com o teto em metade da planta (a torre, a antena e os fios não levantam a barra:
			// na usina ela voava no topo da torre de transmissão)
			const FBox2D Foot = View->GetFootprintCm();
			const FVector Scale = View->GetActorScale3D();
			const float SizeX = float(Foot.GetSize().X * Scale.X), SizeY = float(Foot.GetSize().Y * Scale.Y);
			HalfCm = FMath::Max(300.0f, 0.25f * (SizeX + SizeY));
			const FVector Center = View->GetActorTransform().TransformPosition(FVector(Foot.GetCenter().X, Foot.GetCenter().Y, 0.0));
			TopWorld = FVector(Center.X, Center.Y, Ground.Z - 25.0f + FMath::Min(View->GetHeightCm() * float(Scale.Z), HalfCm));
		}
		else if (View)
		{
			FBox Box(ForceInit);
			View->ForEachComponent<UStaticMeshComponent>(false, [&Box](const UStaticMeshComponent* Part)
			{
				if (Part->IsVisible() && Part->GetStaticMesh() && Part->Bounds.BoxExtent.GetMax() < 5000.0f)
				{
					Box += Part->Bounds.GetBox();
				}
			});
			if (Box.IsValid)
			{
				TopWorld = FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z);
				HalfCm = 0.5f * float(Box.GetExtent().X + Box.GetExtent().Y);
			}
			// (04/11, ele: "achei as barras de saúde muito grandes pras unidades [...] rever a escala de cada uma, em comprimento") a
			// largura sai só do casco, no espaço dele (o cano do canhão e o giro do tanque inflavam a caixa do mundo: o tanque batia
			// no teto de 150 px, mais largo que ele): a barra tem 70% do comprimento do casco na tela
			const FBox2D Hull = View->GetFootprintCm();
			if (Hull.bIsValid)
			{
				const FVector Scale = View->GetActorScale3D();
				HalfCm = 0.35f * float(FMath::Max(Hull.GetSize().X * Scale.X, Hull.GetSize().Y * Scale.Y));
			}
		}
		FVector2D Top, Side;
		if (!PlayerOwner->ProjectWorldLocationToScreen(TopWorld, Top))
		{
			continue;
		}
		const FVector CamRight = PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector) : FVector::RightVector;
		const float RadiusPx = PlayerOwner->ProjectWorldLocationToScreen(TopWorld + CamRight * HalfCm, Side) ? float(FVector2D::Distance(Top, Side)) : 26.0f * S;
		// a régua do quadro aprovado (1600 × 900): o tanque 52 px, o prédio 104, o soldado 26
		const float BarW = FMath::RoundToFloat(U.bStructure ? FMath::Clamp(RadiusPx * 1.2f, 24.0f * S, 150.0f * S) : FMath::Clamp(RadiusPx * 2.0f, 18.0f * S, 110.0f * S));
		const float BarH = FMath::RoundToFloat((U.bStructure ? 6.0f : 5.0f) * S);
		if (View && NewlySelected.Contains(U.Id) && SelectFlashes < 12)
		{
			RTSSelectFlash::Spawn(const_cast<ARTSUnitActor*>(View), Team);
			++SelectFlashes;
		}
		const float X = FMath::RoundToFloat(Top.X - BarW * 0.5f);
		const float Y = FMath::RoundToFloat(Top.Y - 6.0f * S - BarH);
		const FBarChip* Chip = BarChips.Find(U.Id);
		const float ChipFade = Chip && Chip->At >= 0.0 ? 1.0f - FMath::Clamp(float(Now - Chip->At - 0.15) / 0.45f, 0.0f, 1.0f) : 0.0f;
		const int32 Vet = 0;   // a patente da unidade: a simulação ainda não tem (o ZH: sobe com as baixas); o desenho já está pronto
		DrawHealthBar(X, Y, BarW, BarH, S, bBuilding ? U.BuildFraction : Ratio, Chip ? Chip->From : 0.0f, ChipFade, bBuilding, U.MaxHp, Vet, Team);
		if (const double* Pop = SelectPopAt.Find(U.Id))
		{
			// o estalo: a barra acende em branco e volta em 0,22 s
			const float PopT = float(Now - *Pop) / 0.22f;
			if (bSelected && PopT < 1.0f)
			{
				DrawPanel(X - 2.0f, Y - 2.0f, BarW + 4.0f, BarH + 4.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.7f * FMath::Square(1.0f - PopT)));
			}
		}
		// (simulação 35; 01/11, ele: "ficou só quadradinhos mal feitos, mal pensados") a lotação do transporte: a plaquinha presa embaixo
		// da barra de vida, com a mesma moldura do time (as duas leem como uma peça só), e dentro um bonequinho por vaga (a cabeça e os
		// ombros): aceso quem está dentro, apagado a vaga livre. A forma de gente diz "a tropa lá dentro" sem legenda (o garrison dos RTS de
		// hoje); os quadradinhos soltos do ZH (o SCPPipFull/Empty) pediam legenda
		// (01/11; a 1ª e a 2ª REPROVADAS: "horrível, pixelado, sem qualidade, sem criatividade") as lâmpadas da tripulação (DrawSeats), a
		// direção A do Atelier: NÃO aprovada ("eu não aprovei essa imagem aí"), fora do jogo; só com -RTSSeats=1 pra ele ver e escolher
		static const bool bSeats = [] { int32 V = 0; FParse::Value(FCommandLine::Get(), TEXT("RTSSeats="), V); return V != 0; }();
		if (bSeats && U.TransportSlots > 0 && U.Owner == Match->LocalPlayer())
		{
			DrawSeats(X, Y + BarH + 2.0f, BarW, U.TransportSlots, U.Passengers, Team);
		}
	}
}

void ARTSHUD::DrawMarkers(ARTSPlayerController* PC)
{
	// (03/11) a marca da ordem no chão: o anel cortado que fecha, na cor da ordem (RTSOrderMarker.h); era o anel fino de 2 px
	// que encolhia em 0,7 s, que ninguém via
	for (const FRTSOrderMarker& M : PC->GetMarkers())
	{
		RTSOrderMarker::Draw(Canvas, PC, M.World, M.Age, M.Kind, M.RadiusCm, M.bCross);
	}
	RTSOrderMarker::DrawTest(Canvas, PC);
	// (05/11) a mira dos poderes do general no chão (RTSPowerPreview.h): a sessão dos poderes chama o Show a cada quadro
	RTSPowerPreview::Draw(Canvas, PC);
	RTSPowerPreview::DrawTest(Canvas, PC);
}

void ARTSHUD::DrawLinePreview(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	// (v4, 23/10) a prévia da linha dos dois botões: o Total War mostra um fantasma por unidade e o BAR um ponto por unidade ao
	// longo da linha; aqui o anel sai da conta da própria simulação (URTSMatchSubsystem::PreviewLineMove), então fica onde a
	// vaga vai ficar, e não perto. Sem linha ligando a unidade à vaga (não suja a leitura da batalha)
	ARTSPlayerController::FLineDrag L;
	const FRTSBakedMap* Map = Match->BakedMap();
	if (!Map || !PC->GetLineDrag(L))
	{
		return;
	}
	// as cores das marcas de ordem: verde anda, vermelho com o ataque andando armado
	const FLinearColor Color = L.bAttack ? AttackRed : MoveGreen;
	{
		// a linha puxada, fina, em pedaços de ~2 m que seguem o chão (como o DrawGroundRect)
		const int32 Pieces = FMath::Clamp(int32(FVector2D::Distance(L.StartCm, L.EndCm) / 200.0), 1, 64);
		FVector2D Prev = FVector2D::ZeroVector;
		bool bPrev = false;
		for (int32 K = 0; K <= Pieces; ++K)
		{
			const FVector2D P = FMath::Lerp(L.StartCm, L.EndCm, double(K) / Pieces);
			FVector2D Screen;
			const bool bOk = PC->ProjectWorldLocationToScreen(FVector(P.X, P.Y, Map->GroundZCm(P) + 30.0f), Screen);
			if (bOk && bPrev)
			{
				DrawLine(Prev.X, Prev.Y, Screen.X, Screen.Y, WithAlpha(Color, 0.45f), 1.5f);
			}
			Prev = Screen;
			bPrev = bOk;
		}
	}
	TArray<URTSMatchSubsystem::FLineSlot> Slots;
	if (!Match->PreviewLineMove(PC->GetSelection(), L.CenterCm, L.bFormation, L.FacingDeg, L.WidthM, Slots))
	{
		return;
	}
	// cada unidade: o anel no chão onde ela vai parar (60% do raio, entre 1,2 e 4 m: o do tanque dá ~2,2 m e os vizinhos a
	// 8,4 m nunca encostam) e o risquinho pra frente, do anel até 1,8 raio; com mais de 64 o anel cai pra 12 pedaços
	const int32 Segments = Slots.Num() > 64 ? 12 : 28;
	const float Yaw = FMath::DegreesToRadians(L.FacingDeg);
	const FVector2D Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw));
	for (const URTSMatchSubsystem::FLineSlot& Slot : Slots)
	{
		const float R = FMath::Clamp(Slot.RadiusCm * 0.6f, 120.0f, 400.0f);
		DrawGroundRing(FVector(Slot.Cm.X, Slot.Cm.Y, Map->GroundZCm(Slot.Cm) + 30.0f), R, Color, 2.0f, Segments);
		const FVector2D TickFrom = Slot.Cm + Fwd * double(R);
		const FVector2D TickTo = Slot.Cm + Fwd * double(R * 1.8f);
		FVector2D A;
		FVector2D B;
		if (PC->ProjectWorldLocationToScreen(FVector(TickFrom.X, TickFrom.Y, Map->GroundZCm(TickFrom) + 30.0f), A)
			&& PC->ProjectWorldLocationToScreen(FVector(TickTo.X, TickTo.Y, Map->GroundZCm(TickTo) + 30.0f), B))
		{
			DrawLine(A.X, A.Y, B.X, B.Y, Color, 2.0f);
		}
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

FString ARTSHUD::MoneyText(int32 Money)
{
	return FText::Format(LOCTEXT("MoneyValue", "${0}"), FText::AsNumber(Money)).ToString();
}

void ARTSHUD::DrawMoneyRow(FName Prefix, int32 Money, float X, float Y, float W, bool bCanChange, bool bCompact)
{
	// (v4, 22/10) o dinheiro inicial no estilo da QUALITY: [ - ] o valor [ + ], o valor no dourado do dinheiro da partida
	// (o EcoMoney do RTSHUDEconomy.cpp) e um tracinho por escolha embaixo, o de agora aceso
	const FLinearColor Gold(0.98f, 0.82f, 0.30f, 1.0f);
	if (!bCompact)
	{
		DrawLabel(LOCTEXT("StartMoneyTitle", "STARTING MONEY").ToString(), X + 2.0f, Y, 10, DimText);
		Y += 18.0f;
	}
	const float H = bCompact ? 34.0f : 36.0f;
	const FString Value = MoneyText(Money);
	const FVector2D Extent = MeasureLabel(Value, 15, true);
	if (!bCanChange)
	{
		DrawLabel(Value, X + 2.0f, Y + (H - Extent.Y) * 0.5f, 15, Gold, false, true);
		return;
	}
	const float StepW = bCompact ? 36.0f : 44.0f;
	DrawSmallButton(FName(*(Prefix.ToString() + TEXT("_down"))), TEXT("-"), X, Y, StepW, H);
	DrawSmallButton(FName(*(Prefix.ToString() + TEXT("_up"))), TEXT("+"), X + W - StepW, Y, StepW, H);
	const float MidX = X + StepW + 6.0f;
	const float MidW = W - 2.0f * (StepW + 6.0f);
	DrawPanel(MidX, Y, MidW, H, ButtonFill);
	DrawLabel(Value, MidX + (MidW - Extent.X) * 0.5f, Y + (H - 7.0f - Extent.Y) * 0.5f, 15, Gold, false, true);
	const URTSSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<URTSSessionSubsystem>() : nullptr;
	const int32 Count = Session ? Session->MoneyChoices.Num() : 0;
	if (Count > 1)
	{
		const float Gap = 3.0f;
		const float TickW = FMath::Min(14.0f, (MidW - 16.0f - Gap * (Count - 1)) / Count);
		float TickX = MidX + (MidW - (Count * TickW + (Count - 1) * Gap)) * 0.5f;
		for (const int32 Choice : Session->MoneyChoices)
		{
			DrawRect(Choice == Money ? Accent : FLinearColor(1, 1, 1, 0.16f), TickX, Y + H - 7.0f, TickW, 3.0f);
			TickX += TickW + Gap;
		}
	}
}

void ARTSHUD::DrawFactionPicker(FName Prefix, const FString& Title, const FString& Value, float X, float Y, float W)
{
	// (31/10) no estilo do dinheiro inicial: o título pequeno, [<] o nome [>] (o nome em maiúsculas no dourado do menu)
	DrawLabel(Title, X + 2.0f, Y, 10, DimText);
	Y += 18.0f;
	const float H = 36.0f;
	const float StepW = 36.0f;
	DrawSmallButton(FName(*(Prefix.ToString() + TEXT("_down"))), TEXT("<"), X, Y, StepW, H);
	DrawSmallButton(FName(*(Prefix.ToString() + TEXT("_up"))), TEXT(">"), X + W - StepW, Y, StepW, H);
	const float MidX = X + StepW + 4.0f;
	const float MidW = W - 2.0f * (StepW + 4.0f);
	DrawPanel(MidX, Y, MidW, H, ButtonFill);
	const FString Shown = Value.ToUpper();
	const FVector2D Extent = MeasureLabel(Shown, 13, true);
	DrawLabel(Shown, MidX + (MidW - Extent.X) * 0.5f, Y + (H - Extent.Y) * 0.5f, 13, FLinearColor(0.98f, 0.82f, 0.30f, 1.0f), false, true);
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
	// (16/10) o campo de batalha: o mapa com as marcas que a partida deixou, à direita (o meio fica pra última explosão)
	// (o patriot_fim1: com 22% da altura ele invadia o painel da facção, embaixo à direita) acima do painel do HUD
	const float MapHalf = FMath::Min(H * 0.17f, W * 0.11f);
	DrawBattleMap(Match, FVector2D(W - MapHalf - W * 0.05f, Top + H * 0.24f + 40.0f + MapHalf), MapHalf, Alpha);
	if (Match->IsNetMatch())
	{
		// a partida em rede: todos voltam pra mesma sala (o anfitrião leva os outros junto)
		DrawButton(TEXT("lobby"), LOCTEXT("ReturnToLobby", "RETURN TO LOBBY").ToString(), W * 0.5f - 170.0f, Top + H * 0.24f + 18.0f, 340.0f, 54.0f);
		return;
	}
	DrawButton(TEXT("menu"), LOCTEXT("ReturnToMenu", "RETURN TO MENU").ToString(), W * 0.5f - 170.0f, Top + H * 0.24f + 18.0f, 340.0f, 54.0f);
}

void ARTSHUD::DrawBattleMap(URTSMatchSubsystem* Match, const FVector2D& Center, float Half, float Alpha)
{
	URTSFogOfWar* Fog = Match->GetFog();
	UTexture2D* Tex = Fog ? Fog->MinimapTexture() : nullptr;
	if (!Fog || !Fog->IsMinimapReady() || !Tex || !Tex->GetResource() || Alpha <= 0.01f)
	{
		return;
	}
	// o mesmo losango do minimapa (girado pelo giro padrão da câmera), maior
	const double Side = double(Half) * UE_SQRT_2;
	const auto ToScreen = [&](const FVector2D& UV)
	{
		const double X = (UV.X - 0.5) * Side;
		const double Y = (UV.Y - 0.5) * Side;
		return Center + FVector2D(X * MinimapCos + Y * MinimapSin, -X * MinimapSin + Y * MinimapCos);
	};
	const FVector2D C00 = ToScreen(FVector2D(0, 0));
	const FVector2D C10 = ToScreen(FVector2D(1, 0));
	const FVector2D C11 = ToScreen(FVector2D(1, 1));
	const FVector2D C01 = ToScreen(FVector2D(0, 1));
	for (int32 T = 0; T < 2; ++T)
	{
		FCanvasTriangleItem Tri = T == 0
			? FCanvasTriangleItem(C00, C10, C11, FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), Tex->GetResource())
			: FCanvasTriangleItem(C00, C11, C01, FVector2D(0, 0), FVector2D(1, 1), FVector2D(0, 1), Tex->GetResource());
		Tri.BlendMode = SE_BLEND_Translucent;
		Tri.SetColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.92f * Alpha));
		Canvas->DrawItem(Tri);
	}
	if (UTextureRenderTarget2D* Battle = Match->GetBattleOverlay())
	{
		if (FTextureResource* BattleRes = Battle->GetResource())
		{
			for (int32 T = 0; T < 2; ++T)
			{
				// a troca dos eixos do minimapa (o canto (u, v) do minimapa é o (1 − v, u) do caderno)
				FCanvasTriangleItem Tri = T == 0
					? FCanvasTriangleItem(C00, C10, C11, FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1), BattleRes)
					: FCanvasTriangleItem(C00, C11, C01, FVector2D(1, 0), FVector2D(0, 1), FVector2D(0, 0), BattleRes);
				Tri.BlendMode = SE_BLEND_Translucent;
				Tri.SetColor(FLinearColor(1.0f, 1.0f, 1.0f, Alpha));
				Canvas->DrawItem(Tri);
			}
		}
	}
	const FLinearColor Edge(0.62f, 0.66f, 0.72f, 0.7f * Alpha);
	DrawLine(C00.X, C00.Y, C10.X, C10.Y, Edge, 1.5f);
	DrawLine(C10.X, C10.Y, C11.X, C11.Y, Edge, 1.5f);
	DrawLine(C11.X, C11.Y, C01.X, C01.Y, Edge, 1.5f);
	DrawLine(C01.X, C01.Y, C00.X, C00.Y, Edge, 1.5f);
	DrawLabel(LOCTEXT("BattleMap", "THE BATTLEFIELD").ToString(), Center.X, Center.Y - Half - 26.0f, 11, WithAlpha(TextColor, Alpha), true);
	DrawLabel(LOCTEXT("BattleMapLegend", "worn tracks   ·   scorch and craters   ·   burning wrecks").ToString(), Center.X, Center.Y + Half + 10.0f, 9, WithAlpha(DimText, Alpha), true);
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
	// (16/10, as marcas que contam a batalha) por cima da foto: o caminho batido, o queimado, as crateras e o combustível,
	// só o que o jogador viu (a memória do chão só recebe o que estava à vista)
	if (UTextureRenderTarget2D* Battle = Match->GetBattleOverlay())
	{
		if (FTextureResource* BattleRes = Battle->GetResource())
		{
			for (int32 T = 0; T < 2; ++T)
			{
				// a foto do minimapa tem o U no Y do mundo e o V invertido no X (URTSFogOfWar::MinimapUVToWorld); o caderno
				// tem o X no U e o Y no V: o canto (u, v) do minimapa é o (1 − v, u) do caderno
				FCanvasTriangleItem Tri = T == 0
					? FCanvasTriangleItem(C00, C10, C11, FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1), BattleRes)
					: FCanvasTriangleItem(C00, C11, C01, FVector2D(1, 0), FVector2D(0, 1), FVector2D(0, 0), BattleRes);
				Tri.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Tri);
			}
		}
	}
	const FLinearColor Edge(0.62f, 0.66f, 0.72f, 0.55f);
	DrawLine(C00.X, C00.Y, C10.X, C10.Y, Edge, 1.0f);
	DrawLine(C10.X, C10.Y, C11.X, C11.Y, Edge, 1.0f);
	DrawLine(C11.X, C11.Y, C01.X, C01.Y, Edge, 1.0f);
	DrawLine(C01.X, C01.Y, C00.X, C00.Y, Edge, 1.0f);

	// (03/11, os alertas aprovados) o minimapa pros alertas: o calor da batalha por baixo dos sinais; sem energia, o radar cai
	// (o chuvisco por cima e sem os sinais: perder a usina custa a visão, como no ZH)
	RTSAlerts::FView AlertView;
	AlertView.Center = MinimapCenter;
	AlertView.Side = MinimapSide;
	AlertView.PxPerCm = MinimapSide / float(FMath::Max(MinimapSizeCm.X, 1.0));
	AlertView.ToScreen = [this](const FVector2D& Cm) { FVector2D S; WorldToMinimapScreen(Cm, S); return S; };
	{
		const FVector2D Map[4] = { MinimapMinCm, MinimapMinCm + FVector2D(MinimapSizeCm.X, 0.0), MinimapMinCm + MinimapSizeCm, MinimapMinCm + FVector2D(0.0, MinimapSizeCm.Y) };
		for (int32 K = 0; K < 4; ++K)
		{
			AlertView.Corners[K] = AlertView.ToScreen(Map[K]);
		}
	}
	// (04/11, HUD) o radar cai como no ZH (Energy::hasSufficientPower): só gastando mais do que gera. Com 0 / 0 (sem usina no começo, e a
	// GLA, que não usa energia) o radar fica ligado; antes a GLA ficava em RADAR OFFLINE a partida inteira
	const bool bRadarOff = RTSAlerts::TestRadarOff() || (OwnsStructure(Match, Match->LocalPlayer()) && Match->PowerUsed(Match->LocalPlayer()) > Match->PowerMade(Match->LocalPlayer()));
	if (bRadarOff)
	{
		RTSAlerts::DrawRadarOffline(Canvas, AlertView, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, [this](const FString& Text, const FVector2D& At)
		{
			const int32 Size = FMath::RoundToInt(13.0f * FMath::Max(1.0f, Canvas->SizeY / 900.0f));
			DrawLabel(Text, At.X, At.Y - MeasureLabel(Text, Size, true).Y * 0.5f, Size, FLinearColor(FColor(255, 178, 31)), true, true);
		});
	}
	else
	{
		RTSAlerts::DrawHeat(Canvas, AlertView, GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f);
	}
	// unidades (inimigo só à vista), na cor do jogador; selecionado com borda clara
	// ~5 px a 900 linhas: o tanque de 8 m tem 1,5 px no mapa de 1 km, então o ponto é maior que ele
	const float Dot = FMath::Max(4.0f, F / 55.0f);
	// os depósitos de suprimento: estáticos e de todos, então sempre à vista (como a prévia do mapa
	// do ZH marca os SUPPLY_SOURCE); a pilha que caiu só onde se vê
	for (int32 K = 0; K < Match->DockCount() && !bRadarOff; ++K)
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
		// (03/11, o minimapa v4 aprovado) o miolo escuro da caixa: a doca não lê como unidade (o quadrado igual confundia)
		const float In = FMath::Max(1.0f, FMath::RoundToFloat(S * 0.36f));
		DrawPanel(P.X - In * 0.5f, P.Y - In * 0.5f, In, In, FLinearColor(FColor(150, 104, 30)));
	}
	TArray<FRTSUnitState> Units;
	Match->GetUnits(Units);
	const TArray<uint32>& Selected = PC->GetSelection();
	for (const FRTSUnitState& U : Units)
	{
		if (bRadarOff || !Match->IsUnitShown(U.Id))
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
	// (03/11, o minimapa v4 aprovado) a câmera em branco com a sombra escura por baixo: a amarela de 1,5 px sumia na areia
	const float BoxScale = FMath::Max(1.0f, Canvas->SizeY / 900.0f);
	for (int32 K = 0; K < 4; ++K)
	{
		DrawClippedLine(Box[K], Box[(K + 1) % 4], FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), 3.4f * BoxScale);
	}
	for (int32 K = 0; K < 4; ++K)
	{
		DrawClippedLine(Box[K], Box[(K + 1) % 4], FLinearColor(1.0f, 1.0f, 1.0f, 0.95f), 1.6f * BoxScale);
	}

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

	// (03/11, os alertas aprovados) o sob ataque (a unidade e a base), a superarma inimiga, o seu poder pronto, a doca, a carga, o
	// primeiro contato e a perda (RTSAlerts.h), no lugar do triângulo de 1 px do ZH
	for (const FRTSMinimapPing& Ping : PC->GetMinimapPings())
	{
		if (!bRadarOff)   // (03/11, ele: "o radar offline mostrou alertas dentro do minimapa, isso não pode")
		{
			RTSAlerts::DrawPing(Canvas, AlertView, Ping);
		}
	}
	if (PC->AttackMessageAlpha() > 0.0f && !bNewMinimapGeometry)
	{
		DrawLabel(LOCTEXT("UnderAttack", "Unit under attack  (Space to go there)").ToString(), X0 + 2.0f, Y0 - 22.0f, 11, WithAlpha(AttackRed, PC->AttackMessageAlpha()), false, true);
	}
}

#undef LOCTEXT_NAMESPACE
