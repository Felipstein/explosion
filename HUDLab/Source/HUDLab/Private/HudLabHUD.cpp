#include "HudLabHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HudLabStates.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTSHudKit.h"
#include "RTSHudPainter.h"
#include "UnrealClient.h"

void AHudLabHUD::BeginPlay()
{
	Super::BeginPlay();
	RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
	FParse::Value(FCommandLine::Get(), TEXT("State="), StateIndex);
	StateIndex = FMath::Clamp(StateIndex, 1, 6);
	FParse::Value(FCommandLine::Get(), TEXT("Shot="), ShotPath);
	FParse::Value(FCommandLine::Get(), TEXT("Cols="), MainColumns);
	FParse::Value(FCommandLine::Get(), TEXT("Seq="), SeqFrames);
	bMeasure = FParse::Param(FCommandLine::Get(), TEXT("Measure"));
	FString FactionArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("Faction="), FactionArg) && FactionArg != TEXT("usa"))
	{
		SkinFaction = FName(*FactionArg);
	}
	FParse::Value(FCommandLine::Get(), TEXT("Time="), SimTime);
	if (PlayerOwner)
	{
		PlayerOwner->bShowMouseCursor = true;
	}
	if (!ShotPath.IsEmpty() && GEngine)
	{
		// a foto sai em 1920×1080 (a planta), ou no -Res=LxA pedido (o teste das outras telas)
		FString Res = TEXT("1920x1080");
		FParse::Value(FCommandLine::Get(), TEXT("Res="), Res);
		GEngine->Exec(GetWorld(), *FString::Printf(TEXT("r.SetRes %sw"), *Res));
	}
	Kit = MakeShared<FRTSHudKit>(RepoRoot / TEXT("docs/concept/hud/kit"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("LabAssets")));
	Painter = MakeShared<FRTSHudPainter>([this](FName N) { return GetTexture(N); }, FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Fonts")));
}

UTexture2D* AHudLabHUD::GetTexture(FName Name)
{
	return Kit ? Kit->Get(Name) : nullptr;
}

void AHudLabHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !Painter)
	{
		return;
	}

	if (PlayerOwner)
	{
		const FKey Keys[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
		for (int32 I = 0; I < 6; ++I)
		{
			if (PlayerOwner->WasInputKeyJustPressed(Keys[I]))
			{
				StateIndex = I + 1;
			}
		}
	}

	// o fundo: a cena aprovada, enchendo a tela
	if (UTexture2D* Bg = GetTexture(TEXT("bg")))
	{
		if (Bg->GetResource())
		{
			const float SA = Canvas->ClipX / Canvas->ClipY, TA = float(Bg->GetSizeX()) / Bg->GetSizeY();
			FVector2D UV0(0, 0), UV1(1, 1);
			if (TA > SA) { const float U = SA / TA; UV0.X = (1 - U) * 0.5f; UV1.X = 1 - UV0.X; }
			else { const float V = TA / SA; UV0.Y = (1 - V) * 0.5f; UV1.Y = 1 - UV0.Y; }
			FCanvasTileItem Item(FVector2D(0, 0), Bg->GetResource(), FVector2D(Canvas->ClipX, Canvas->ClipY), UV0, UV1, FLinearColor::White);
			Canvas->DrawItem(Item);
		}
	}

	FRTSHudState State = MakeHudLabState(StateIndex, MainColumns);
	// os jogadores (03/11): -Format=3v5v8 troca os jogadores pelos de um formato do protótipo aprovado (os mesmos números); -Abuse dá 240
	// superarmas a um inimigo; -Me=RRGGBB troca a sua cor; -Tab abre o placar (na janela: segurar o Tab); -Scroll=N rola a coluna das superarmas
	{
		FString Format, MeHex;
		if (FParse::Value(FCommandLine::Get(), TEXT("Format="), Format))
		{
			FParse::Value(FCommandLine::Get(), TEXT("Me="), MeHex);
			ApplyHudLabFormat(State, Format, SkinFaction.IsNone() ? FString(TEXT("usa")) : SkinFaction.ToString(), FParse::Param(FCommandLine::Get(), TEXT("Abuse")), MeHex);
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("NoSw")))
		{
			// -NoSw: a partida sem nenhuma superarma (a lista C some e o placar fica sem a coluna)
			for (FRTSHudPlayer& P : State.Players)
			{
				P.Superweapons.Reset();
			}
		}
		State.bScoreboard = FParse::Param(FCommandLine::Get(), TEXT("Tab")) || (PlayerOwner && PlayerOwner->IsInputKeyDown(EKeys::Tab));
		FParse::Value(FCommandLine::Get(), TEXT("Scroll="), State.ScoreSwScroll);
	}
	const bool bSeq = !ShotPath.IsEmpty() && SeqFrames > 0;
	if (ShotPath.IsEmpty())
	{
		// na janela: o tempo de verdade, e o mouse de verdade (em cima e apertando)
		SimTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f;
		State.bDrawCursor = false;
		State.TooltipFor = NAME_None;
		if (PlayerOwner)
		{
			float MX = 0, MY = 0;
			if (PlayerOwner->GetMousePosition(MX, MY))
			{
				for (const FRTSHudButton& B : LastButtons)
				{
					if (B.Id != TEXT("hud_bg") && B.Box.IsInside(FVector2D(MX, MY)))
					{
						State.HoverId = B.Id;
						State.TooltipFor = B.Id;
						break;
					}
				}
			}
			if (PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton))
			{
				State.PressedId = State.HoverId;
			}
		}
	}
	else
	{
		// na foto: o mouse está na casa da dica
		State.HoverId = State.TooltipFor;
	}
	State.Time = SimTime;
	State.SkinFaction = SkinFaction;
	// -Bg: a foto de fundo do protótipo do console (captures/concept/hud/recursos2/bg_*): sem o painel do dinheiro antigo, sem as
	// abas dos grupos e sem a dica, que o protótipo desenha por cima do jeito novo
	if (FParse::Param(FCommandLine::Get(), TEXT("Bg")))
	{
		State.Money.Empty();
		State.Groups.Empty();
		State.TooltipFor = NAME_None;
		State.HoverId = NAME_None;
	}
	// -TopEmblem=0|1|2: o emblema no topo (1 a silhueta na cor do jogador, aprovada e o padrão; 0 o inteiro; 2 a ficha nas cores da facção)
	{
		int32 TE = 1;
		FParse::Value(FCommandLine::Get(), TEXT("TopEmblem="), TE);
		State.TopEmblem = uint8(FMath::Clamp(TE, 0, 2));
	}
	// -Colors=RRGGBB,RRGGBB: a cor de você e do adversário na foto (a prova de que o topo segue a cor de cada jogador)
	FString ColorsArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("Colors="), ColorsArg, false))   // false: a vírgula separa as duas cores, não corta a opção
	{
		TArray<FString> Hex;
		ColorsArg.ParseIntoArray(Hex, TEXT(","));
		for (int32 I = 0; I < Hex.Num() && I < State.Players.Num(); ++I)
		{
			State.Players[I].Color = FLinearColor(FColor::FromHex(Hex[I]));
		}
	}
	if (!SkinFaction.IsNone())
	{
		// os estados do laboratório são montados como USA: a placa e o seu emblema passam pra facção da foto
		const FString F = SkinFaction.ToString();
		const FName Emb(*FString::Printf(TEXT("emblem:%s"), *F));
		State.Card.FactionEmblem = Emb;
		State.Card.FactionName = F == TEXT("china") ? TEXT("CHINA") : F == TEXT("gla") ? TEXT("GLA") : F == TEXT("russia") ? TEXT("RUSSIA") : F.ToUpper();
		for (FRTSHudPlayer& P : State.Players)
		{
			if (P.bMe)
			{
				P.Emblem = Emb;
			}
		}
	}
	if (bSeq)
	{
		// o vídeo: o painel sobe e o aviso entra no começo; a casa da dica é apertada em 1,2 s; o carregamento anda
		State.PanelIn = FMath::Clamp(SimTime / 0.35f, 0.0f, 1.0f);
		for (FRTSHudAlert& A : State.Alerts)
		{
			A.Age = SimTime - 0.4f;
		}
		if (SimTime > 1.2f && SimTime < 1.4f)
		{
			State.PressedId = State.TooltipFor;
		}
		auto Advance = [this](FRTSHudSlot& Sl)
		{
			if (Sl.Progress >= 0.0f)
			{
				Sl.Progress = FMath::Fmod(Sl.Progress + SimTime / 6.0f, 1.0f);
			}
		};
		for (FRTSHudSlot& Sl : State.Main) Advance(Sl);
		for (FRTSHudSlot& Sl : State.Card.Queue) Advance(Sl);
		for (FRTSHudPower& Pw : State.Powers)
		{
			if (Pw.Remaining > 0.0f)
			{
				Pw.Remaining = FMath::Max(0.0f, Pw.Remaining - SimTime / 8.0f);
			}
		}
	}
	const double T0 = FPlatformTime::Seconds();
	Painter->Draw(Canvas, State, LastButtons);
	if (Frames >= 60)
	{
		// só depois das imagens carregadas (a primeira vez que o painter pede uma textura, ela é lida do disco)
		DrawMs += (FPlatformTime::Seconds() - T0) * 1000.0;
		++DrawCount;
	}

	// a foto automática: espera as imagens carregarem, tira e fecha
	++Frames;
	if (bSeq)
	{
		// a sequência: um quadro por foto, o tempo andando 1/30 s por foto (o vídeo sai igual em qualquer máquina)
		const FString Path = FPaths::GetPath(ShotPath) / FString::Printf(TEXT("%s_%03d.png"), *FPaths::GetBaseFilename(ShotPath), SeqIndex);
		if (Frames >= 40 && !bShotRequested)
		{
			FScreenshotRequest::RequestScreenshot(Path, true, false);
			bShotRequested = true;
			SeqWait = 0;
		}
		else if (bShotRequested && (IFileManager::Get().FileExists(*Path) || ++SeqWait > 200))
		{
			bShotRequested = false;
			++SeqIndex;
			SimTime = SeqIndex / 30.0f;
			if (SeqIndex >= SeqFrames)
			{
				FPlatformMisc::RequestExit(false);
			}
		}
	}
	else if (!ShotPath.IsEmpty())
	{
		if (!bShotRequested && Frames >= (bMeasure ? 300 : 40))
		{
			FScreenshotRequest::RequestScreenshot(ShotPath, true, false);
			bShotRequested = true;
		}
		else if (bShotRequested && (IFileManager::Get().FileExists(*ShotPath) || Frames > 400))
		{
			if (Frames > 400)
			{
				UE_LOG(LogTemp, Error, TEXT("HUDLab: a foto não saiu: %s"), *ShotPath);
			}
			UE_LOG(LogTemp, Display, TEXT("HUDLab: custo do HUD (CPU do Draw): %.3f ms por quadro, média de %d quadros"), DrawCount ? DrawMs / DrawCount : 0.0, DrawCount);
			FPlatformMisc::RequestExit(false);
		}
	}
}
