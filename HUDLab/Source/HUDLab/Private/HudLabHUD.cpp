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
		// (05/11) C troca a versão das casas (A como está, B limpa); M deixa o dinheiro curto ($900)
		if (PlayerOwner->WasInputKeyJustPressed(EKeys::C)) bLabClean = !bLabClean;
		if (PlayerOwner->WasInputKeyJustPressed(EKeys::M)) bLabPoor = !bLabPoor;
		if (PlayerOwner->WasInputKeyJustPressed(EKeys::V)) bLabHideEmpty = !bLabHideEmpty;
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
	// (04/11) o tempo do hover: na janela, desde que o mouse entrou no botão; na foto, o -HoverAge pedido (0,25 s: a luz passando no meio)
	if (ShotPath.IsEmpty())
	{
		if (State.HoverId != LastHoverId) { LastHoverId = State.HoverId; HoverSince = SimTime; }
		if (State.PressedId != LastPressId) { LastPressId = State.PressedId; PressSince = SimTime; }
		State.HoverAge = SimTime - HoverSince;
		State.PressAge = SimTime - PressSince;
	}
	else
	{
		State.HoverAge = 0.25f;
		FParse::Value(FCommandLine::Get(), TEXT("HoverAge="), State.HoverAge);
		if (FParse::Param(FCommandLine::Get(), TEXT("Press"))) { State.PressedId = State.HoverId; State.PressAge = 1.0f; }
	}
	// (05/11) -Poor: o dinheiro curto ($900): as casas que custam mais e não estão trancadas ficam sem dinheiro (o preço vermelho; na versão
	// limpa, apagadas)
	const bool bPoor = bLabPoor || FParse::Param(FCommandLine::Get(), TEXT("Poor"));
	if (bPoor)
	{
		State.Money = TEXT("$ 900");
		for (FRTSHudSlot& Sl : State.Main)
		{
			const int32 Cost = FCString::Atoi(*Sl.Price.Replace(TEXT("$"), TEXT("")).Replace(TEXT(","), TEXT("")));
			Sl.bNoMoney = !Sl.bLocked && Cost > 900;
		}
	}
	// (05/11) -Clean: a versão limpa das casas (sem a tecla e sem o preço; a dica mostra)
	State.bCleanSlots = bLabClean || FParse::Param(FCommandLine::Get(), TEXT("Clean"));
	State.bHideEmptySlots = bLabHideEmpty || FParse::Param(FCommandLine::Get(), TEXT("HideEmpty"));
	// (05/11) na janela: a dica da casa debaixo do mouse (o nome, a tecla, o preço com o tempo e o motivo de não poder)
	if (ShotPath.IsEmpty() && !State.TooltipFor.IsNone())
	{
		const FRTSHudSlot* Hot = State.Main.FindByPredicate([&](const FRTSHudSlot& Sl) { return Sl.Id == State.TooltipFor && !Sl.bEmpty; });
		if (!Hot) Hot = State.Actions.FindByPredicate([&](const FRTSHudSlot& Sl) { return Sl.Id == State.TooltipFor && !Sl.bEmpty; });
		if (Hot)
		{
			FString Cost, Line;
			bool bRed = false;
			if (!HudLabSlotTip(Hot->Id, Cost, Line, bRed)) { Cost = Hot->Price; }
			if (bPoor && Hot->bNoMoney && !Hot->bLocked)
			{
				const int32 Price = FCString::Atoi(*Hot->Price.Replace(TEXT("$"), TEXT("")).Replace(TEXT(","), TEXT("")));
				Line = FString::Printf(TEXT("Not enough money: need $%s more"), *FText::AsNumber(Price - 900).ToString());
				bRed = true;
			}
			State.TooltipTitle = Hot->Label;
			State.TooltipKey = Hot->Key ? FString::Chr(Hot->Key) : FString();
			State.TooltipCost = Cost;
			State.TooltipLine = Line;
			State.bTooltipLineRed = bRed;
		}
	}
	// (05/11) -NoCursor: a foto sem o mouse do laboratório e sem a dica (o que fica embaixo do cursor, como o raio da casa em produção)
	if (FParse::Param(FCommandLine::Get(), TEXT("NoCursor")))
	{
		State.bDrawCursor = false;
		State.TooltipFor = NAME_None;
		State.HoverId = NAME_None;
	}
	// (05/11) -Brown (ou o estado 5): o apagão do console; na foto -BrownAge=6 (o firme), -BrownAge=0.25 (o fundo do tranco)
	if (FParse::Param(FCommandLine::Get(), TEXT("Brown")))
	{
		State.BrownAge = ShotPath.IsEmpty() ? FMath::Fmod(SimTime, 12.0f) : 6.0f;
		FParse::Value(FCommandLine::Get(), TEXT("BrownAge="), State.BrownAge);
		State.PowerUsed = FMath::Max(State.PowerUsed, State.PowerMade + 2);
	}
	// (05/11) -MoneyDemo: o "normal" do protótipo: entra $1,200 (voa até o número, que rola 0,9 s depois de 1,05 s) e sai $900 (rola 0,5 s)
	if (FParse::Param(FCommandLine::Get(), TEXT("MoneyDemo")))
	{
		const float T = FMath::Fmod(SimTime, 5.2f);
		State.MoneyChips.Add({ TEXT("+$1,200"), true, T });
		if (T >= 2.6f)
		{
			State.MoneyChips.Add({ TEXT("\u2212$900"), false, T - 2.6f });
		}
		const auto Out3 = [](float K) { K = FMath::Clamp(K, 0.0f, 1.0f); return 1.0f - FMath::Pow(1.0f - K, 3.0f); };
		const int32 V = FMath::RoundToInt(4250.0f + 1200.0f * Out3((T - 1.05f) / 0.9f) - 900.0f * Out3((T - 2.6f) / 0.5f));
		State.Money = TEXT("$ ") + FText::AsNumber(V).ToString();
	}
	// (04/11) -Chat: a demonstração do protótipo (o chatDemo, num ciclo de 16 s): o Bruno aos aliados, a derrota da IA, o Caio, e a
	// caixa aberta digitando "on my way, 2 min" (uma letra a cada 0,1 s), que vira a sua linha; na janela anda sozinha, na foto -Time=7
	if (FParse::Param(FCommandLine::Get(), TEXT("Chat")))
	{
		const float T = FMath::Fmod(SimTime, 16.0f);
		FRTSHudChat& Ch = State.Chat;
		auto Line = [&](float At, const TCHAR* Who, const TCHAR* Text, FColor C, bool bAllies, bool bSystem)
		{
			if (T < At) return;
			FRTSHudChatLine L;
			L.Who = Who; L.Text = Text; L.Color = FLinearColor(C); L.bAllies = bAllies; L.bSystem = bSystem; L.Age = T - At;
			Ch.Lines.Add(L);
			Ch.PingAge = T - At;
		};
		const FString Reply = TEXT("on my way, 2 min");
		const float TypeAt = 5.6f, TypeEnd = TypeAt + Reply.Len() * 0.1f + 0.7f;
		Line(0.4f, TEXT("Bruno"), TEXT("pushing mid with 6 tanks, cover me"), FColor(230, 236, 243), true, false);
		Line(2.2f, TEXT(""), TEXT("Hard AI 4 has been defeated"), FColor::White, false, true);
		Line(3.8f, TEXT("Caio"), TEXT("gg, your Patriots are toast"), FColor(123, 214, 111), false, false);
		Line(TypeEnd, TEXT("Felipe"), *Reply, FColor(95, 176, 255), true, false);
		Ch.bTyping = T >= TypeAt && T < TypeEnd;
		Ch.bAllies = true;
		Ch.TypingAge = T - TypeAt;
		Ch.Draft = Reply.Left(FMath::Clamp(int32((T - TypeAt) / 0.1f), 0, Reply.Len()));
	}
	// (04/11) -General: a tela do general aberta, com a árvore da facção (Game/Config/RTS/GeneralTrees.json); -GenRank=3 a patente, -Own=drone,para1
	// os comprados (o padrão: os da foto aprovada) e -Hover=desig o poder no detalhe
	if (FParse::Param(FCommandLine::Get(), TEXT("General")))
	{
		FRTSHudGeneral& G = State.General;
		G.LoadTree(RepoRoot / TEXT("Game/Config/RTS/GeneralTrees.json"), SkinFaction.IsNone() ? FString(TEXT("usa")) : SkinFaction.ToString());
		G.bOpen = true;
		G.bLegible = FParse::Param(FCommandLine::Get(), TEXT("GenB"));   // (05/11) -GenB: a versão legível
		FParse::Value(FCommandLine::Get(), TEXT("GenRank="), G.Rank);
		G.Rank = FMath::Clamp(G.Rank, 1, 5);
		FString Own = TEXT("drone,para1,naval1"), Hover;
		FParse::Value(FCommandLine::Get(), TEXT("Own="), Own, false);
		TArray<FString> Ids;
		Own.ParseIntoArray(Ids, TEXT(","));
		for (const FString& Id : Ids) G.Owned.Add(FName(*Id));
		G.PointsLeft = G.PointsLeftFor(G.Rank);
		static const int32 XpNext[5] = { 600, 1500, 3000, 5000, 8000 };
		G.NextFrac = G.Rank < 5 ? 0.35f : 1.0f;
		G.XpLine = G.Rank < 5 ? FString::Printf(TEXT("%s / %s XP"), *FText::AsNumber(FMath::RoundToInt(XpNext[G.Rank] * 0.35f)).ToString(), *FText::AsNumber(XpNext[G.Rank]).ToString()) : FString(TEXT("max XP"));
		// as comendas do protótipo
		const FString Fac = SkinFaction.IsNone() ? FString() : SkinFaction.ToString() + TEXT("/");
		auto Medal = [&](const TCHAR* Name, const TCHAR* Line, float Prog, bool bDone, int32 Xp, const TCHAR* Key)
		{
			FRTSHudGenMedal M; M.Name = Name; M.Line = Line; M.Progress = Prog; M.bDone = bDone; M.Xp = Xp;
			M.Icon = FName(*FString::Printf(TEXT("skin:%smedal_%s"), *Fac, Key));
			G.Medals.Add(M);
		};
		Medal(TEXT("First Strike"), TEXT("First enemy vehicle"), -1.0f, true, 150, TEXT("first_strike"));
		Medal(TEXT("Tank Killer"), TEXT("10 vehicles · 7 / 10"), 0.7f, false, 300, TEXT("tank_killer"));
		Medal(TEXT("Hold the Line"), TEXT("Base under attack · 2:10 / 3:00"), 0.72f, false, 400, TEXT("hold_the_line"));
		Medal(TEXT("Logistics"), TEXT("$10,000 delivered · 6,450"), 0.64f, false, 250, TEXT("logistics"));
		if (FParse::Value(FCommandLine::Get(), TEXT("Hover="), Hover))
		{
			State.HoverId = FName(*(TEXT("gen_") + Hover));
		}
		State.TooltipFor = NAME_None;
	}
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
	if (ShotPath.IsEmpty() && GEngine)
	{
		// (05/11) na janela, o lembrete do laboratório (não é do jogo): qual versão está na tela e as teclas
		const FString Lab = FString::Printf(TEXT("%s%s   |   C: A / B   |   V: casas vazias   |   M: $900   |   1-6: estados (2 = todos os casos)"),
			State.bCleanSlots ? TEXT("B  limpa") : TEXT("A  como esta hoje"), State.bHideEmptySlots ? TEXT(", sem as vazias") : TEXT(""));
		FCanvasTextItem T(FVector2D(Canvas->ClipX * 0.5f - 440.0f, Canvas->ClipY * 0.105f), FText::FromString(Lab), GEngine->GetMediumFont(), State.bCleanSlots ? FLinearColor(1.0f, 0.8f, 0.3f) : FLinearColor::White);
		T.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(T);
	}
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
