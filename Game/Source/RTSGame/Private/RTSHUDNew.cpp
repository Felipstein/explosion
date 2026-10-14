// O HUD novo da sessão de arte (13/10, o plugin RTSHud do HUDLab) no jogo, pro teste do Felipe jogando.
// Uma chave, sem trocar o HUD de hoje (HUDLab/INTEGRACAO.md): -NewHud na linha de comando, e F4 troca
// na partida. O plugin só desenha um FRTSHudState; aqui ele é preenchido com a partida (as mesmas chamadas
// do RTSHUDEconomy.cpp) e os cliques voltam pelos ids de sempre (eco_produce_<def>, eco_build_<def>,
// eco_cancelq_<k>, eco_cancelbuild_0) e pelos novos (group_<n>, queue_<i>, unit_<id>, cmd_*, mm_*).
//
// O minimapa continua o do jogo (o losango alinhado com a câmera padrão, o que o Felipe aprovou em 08/10,
// com a névoa, os pontos, a caixa da câmera e o clique): ele é desenhado por baixo, na geometria do
// losango da peça nova, e a moldura dela vem por cima.
//
// As imagens e as fontes saem do disco (docs/concept/hud/kit, HUDLab/LabAssets, HUDLab/Fonts), como no
// laboratório: serve o teste no editor; no pacote elas viram assets (INTEGRACAO.md, "o que muda").

#include "RTSHUD.h"

#include "Engine/Canvas.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTSHudKit.h"
#include "RTSHudPainter.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"

#define LOCTEXT_NAMESPACE "RTSHUD"

namespace
{
	// a foto do botão e o retrato do cartão (docs/concept/hud/kit/photos) pelo key do Units.json
	FString PhotoName(const FString& Key)
	{
		return Key == TEXT("usa_mbt") ? FString(TEXT("usa_battle_tank")) : Key;
	}

	FString Money(int32 Amount)
	{
		return FString::Printf(TEXT("$%s"), *FText::AsNumber(Amount).ToString());
	}

	/** "$900 · 0:12": o custo e o tempo de fazer, como na dica aprovada (docs/concept/hud). */
	FString CostAndTime(int32 Cost, float Seconds)
	{
		const int32 S = FMath::RoundToInt(Seconds);
		return FString::Printf(TEXT("%s · %d:%02d"), *Money(Cost), S / 60, S % 60);
	}

	/** A dica de cada casa: o custo com o tempo e a linha de baixo (vermelha quando falta prédio). */
	struct FTip
	{
		FString Cost;
		FString Line;
		bool bRed = false;
	};

	bool HasFile(const FString& Kit, const FString& Rel)
	{
		return FPaths::FileExists(Kit / Rel);
	}
}

void ARTSHUD::FillNewHud(ARTSPlayerController* PC, URTSMatchSubsystem* Match, FRTSHudState& S)
{
	const uint8 Me = Match->LocalPlayer();
	const FString KitDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../docs/concept/hud/kit"));
	const auto Btn = [&KitDir](const FString& Key)
	{
		const FString N = PhotoName(Key);
		return HasFile(KitDir, FString::Printf(TEXT("photos/buttons/%s.png"), *N)) ? FName(*(TEXT("btn:") + N)) : FName();
	};
	const auto Card = [&KitDir](const FString& Key)
	{
		const FString N = PhotoName(Key);
		return HasFile(KitDir, FString::Printf(TEXT("photos/card/%s.png"), *N)) ? FName(*(TEXT("card:") + N)) : FName();
	};

	// o topo: você e os aliados primeiro (todos USA por enquanto: o emblema é o da facção)
	TArray<uint8> Order;
	for (int32 P = 0; P < Match->PlayerCount(); ++P)
	{
		Order.Add(uint8(P));
	}
	Order.Sort([&](uint8 A, uint8 B)
	{
		const int32 RA = A == Me ? 0 : (Match->IsAllyOfLocal(A) ? 1 : 2);
		const int32 RB = B == Me ? 0 : (Match->IsAllyOfLocal(B) ? 1 : 2);
		return RA != RB ? RA < RB : A < B;
	});
	for (const uint8 P : Order)
	{
		FRTSHudPlayer H;
		H.Name = P == Me ? LOCTEXT("PlayerYou", "You").ToString() : URTSMatchSubsystem::PlayerColorName(P).ToString();
		H.Emblem = TEXT("emblem:usa");
		H.Color = URTSMatchSubsystem::PlayerColor(P);
		H.bMe = P == Me;
		H.bAlly = P != Me && Match->IsAllyOfLocal(P);
		H.bDead = Match->IsDefeated(P);
		if (H.bAlly)
		{
			H.bShowEco = true;
			H.Money = Money(Match->Money(P));
			H.PowerUsed = Match->PowerUsed(P);
			H.PowerMade = Match->PowerMade(P);
		}
		S.Players.Add(H);
	}
	const int32 Seconds = int32(Match->MatchSeconds());
	S.Clock = FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60);
	S.Money = FString::Printf(TEXT("$ %s"), *FText::AsNumber(Match->Money(Me)).ToString());
	S.PowerUsed = Match->PowerUsed(Me);
	S.PowerMade = Match->PowerMade(Me);
	// (13/10, o plugin novo) o tempo das pulsações, a subida do painel quando a seleção aparece e a idade dos avisos
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	S.Time = float(Now);
	if (PC->GetSelection().IsEmpty())
	{
		NewSelectionShownAt = -1.0;
	}
	else if (NewSelectionShownAt < 0.0)
	{
		NewSelectionShownAt = Now;
	}
	S.PanelIn = NewSelectionShownAt < 0.0 ? 1.0f : FMath::Min(1.0f, float(Now - NewSelectionShownAt) / 0.35f);
	if (PC->AttackMessageAlpha() > 0.0f)
	{
		FRTSHudAlert& A = S.Alerts.Add_GetRef({ LOCTEXT("AlertUnderAttack", "Unit under attack").ToString(), FLinearColor(FColor(224, 68, 58)) });
		A.Age = PC->AttackMessageSeconds();
	}
	// (a regra do Felipe, 10/10: a produção mais lenta aparece; sem prédio nenhum, a energia 0/0 não é aviso, 13/10)
	const bool bLowPower = OwnsStructure(Match, Me) && Match->ProductionSpeed(Me) < 0.999f;
	NewLowPowerSince = bLowPower ? (NewLowPowerSince < 0.0 ? Now : NewLowPowerSince) : -1.0;
	if (bLowPower)
	{
		FRTSHudAlert& A = S.Alerts.Add_GetRef({ LOCTEXT("AlertLowPower", "Low power").ToString(), FLinearColor(FColor(240, 160, 32)) });
		A.Age = float(Now - NewLowPowerSince);
	}
	// o minimapa é desenhado pelo jogo, por baixo (a peça só põe a moldura e os botões)
	S.MinimapImage = NAME_None;

	// os grupos 1–0: a quantidade viva e a vida somada
	S.Groups.SetNum(10);
	for (const TPair<int32, TArray<uint32>>& G : PC->GetGroups())
	{
		if (!S.Groups.IsValidIndex(G.Key))
		{
			continue;
		}
		int32 Hp = 0;
		int32 Max = 0;
		for (const uint32 Id : G.Value)
		{
			FRTSUnitState U;
			if (Match->GetUnit(Id, U) && U.Hp > 0)
			{
				++S.Groups[G.Key].Count;
				Hp += U.Hp;
				Max += U.MaxHp;
			}
		}
		S.Groups[G.Key].Health = Max > 0 ? float(Hp) / Max : 1.0f;
	}
	const TArray<uint32>& Selected = PC->GetSelection();
	S.bGroupsAtBottom = Selected.IsEmpty();
	S.MainColumns = 7;

	// o painel central e o cartão
	const int32 Cash = Match->Money(Me);
	const auto Slot = [](FName Id, FName Image, const FString& Label, const FString& Price)
	{
		FRTSHudSlot X;
		X.bEmpty = false;
		X.Id = Id;
		X.Image = Image;
		X.Label = Label;
		X.Price = Price;
		return X;
	};
	const auto Action = [&Slot](FName Id, TCHAR Key, const FString& Label, FName Icon)
	{
		FRTSHudSlot X = Slot(Id, Icon, Label, FString());
		X.Key = Key;
		X.bAction = true;
		return X;
	};
	TArray<FRTSHudSlot> Main;
	TMap<FName, FTip> Tips;
	// "Requires: War Factory": os prédios que o tipo pede
	const auto Requires = [Match](const FRTSDefInfo& D)
	{
		TArray<FString> Names;
		for (const uint16 R : D.Requires)
		{
			FRTSDefInfo RD;
			if (Match->GetDef(R, RD))
			{
				Names.Add(RD.Name);
			}
		}
		return FText::Format(LOCTEXT("TipRequires", "Requires: {0}"), FText::FromString(FString::Join(Names, TEXT(", ")))).ToString();
	};
	S.Actions.SetNum(6);
	const uint32 StructureId = PC->SelectedStructure();
	TArray<uint32> Dozers;
	PC->SelectedWith(true, Dozers);
	if (StructureId != 0)
	{
		FRTSUnitState U;
		FRTSDefInfo D;
		if (Match->GetUnit(StructureId, U) && Match->GetDef(U.Def, D))
		{
			S.bCommandPanel = true;
			S.Card.bShow = true;
			S.Card.Name = D.Name;
			S.Card.Portrait = Card(D.Key);
			S.Card.Health = U.Hp;
			S.Card.MaxHealth = U.MaxHp;
			if (U.BuildFraction < 1.0f)
			{
				S.Actions[5] = Action(TEXT("eco_cancelbuild_0"), 0, LOCTEXT("CancelConstructionShort", "Cancel Construction").ToString(), TEXT("icon:sell"));
			}
			else
			{
				TArray<uint16> Queue;
				Match->GetQueue(StructureId, Queue);
				for (const uint16 P : D.Produces)
				{
					FRTSDefInfo PD;
					if (!Match->GetDef(P, PD))
					{
						continue;
					}
					FRTSHudSlot X = Slot(FName(*FString::Printf(TEXT("eco_produce_%d"), P)), Btn(PD.Key), PD.Name, Money(PD.Cost));
					X.Key = TEXT('Q');   // (a letra de verdade sai da posição da casa, embaixo)
					for (const uint16 Q : Queue)
					{
						X.Count += Q == P ? 1 : 0;
					}
					if (!Queue.IsEmpty() && Queue[0] == P)
					{
						X.Progress = U.QueueFraction;
					}
					X.bLocked = !Match->HasPrerequisites(P);
					X.bNoMoney = Cash < PD.Cost;
					Tips.Add(X.Id, X.bLocked ? FTip{ CostAndTime(PD.Cost, PD.BuildSeconds), Requires(PD), true }
						: FTip{ CostAndTime(PD.Cost, PD.BuildSeconds), LOCTEXT("TipProduce", "Left-click +1 · Shift-click +5 · Right-click −1").ToString(), false });
					Main.Add(X);
				}
				for (int32 K = 0; K < Queue.Num() && K < 5; ++K)
				{
					FRTSDefInfo QD;
					Match->GetDef(Queue[K], QD);
					FRTSHudSlot X = Slot(FName(*FString::Printf(TEXT("queue_%d"), K)), Btn(QD.Key), QD.Name, FString());
					if (K == 0)
					{
						X.Progress = U.QueueFraction;
					}
					S.Card.Queue.Add(X);
				}
				if (!S.Card.Queue.IsEmpty())
				{
					S.Card.Queue.SetNum(5);
				}
				S.Actions[3] = Action(TEXT("cmd_rally"), 0, LOCTEXT("RallyPoint", "Rally Point").ToString(), TEXT("icon:rally_point"));
			}
		}
	}
	else if (!Dozers.IsEmpty())
	{
		FRTSUnitState U;
		FRTSDefInfo D;
		if (Match->GetUnit(Dozers[0], U) && Match->GetDef(U.Def, D))
		{
			S.bCommandPanel = true;
			S.Card.bShow = true;
			S.Card.Name = Dozers.Num() > 1 ? FString::Printf(TEXT("%s  x%d"), *D.Name, Dozers.Num()) : D.Name;
			S.Card.Portrait = Card(D.Key);
			S.Card.Health = U.Hp;
			S.Card.MaxHealth = U.MaxHp;
			for (const uint16 B : D.Constructs)
			{
				FRTSDefInfo BD;
				if (!Match->GetDef(B, BD))
				{
					continue;
				}
				FRTSHudSlot X = Slot(FName(*FString::Printf(TEXT("eco_build_%d"), B)), Btn(BD.Key), BD.Name, Money(BD.Cost));
				X.Key = TEXT('Q');
				X.bLocked = !Match->HasPrerequisites(B);
				X.bNoMoney = Cash < BD.Cost;
				Tips.Add(X.Id, X.bLocked ? FTip{ CostAndTime(BD.Cost, BD.BuildSeconds), Requires(BD), true } : FTip{ CostAndTime(BD.Cost, BD.BuildSeconds), FString(), false });
				Main.Add(X);
			}
			S.Actions[1] = Action(TEXT("cmd_stop"), TEXT('S'), LOCTEXT("Stop", "Stop").ToString(), TEXT("icon:stop"));
		}
	}
	else if (!Selected.IsEmpty())
	{
		// as unidades: uma casa por unidade (a primeira é a principal), as ordens ao lado
		bool bFirst = true;
		for (const uint32 Id : Selected)
		{
			FRTSUnitState U;
			FRTSDefInfo D;
			if (!Match->GetUnit(Id, U) || !Match->GetDef(U.Def, D) || U.Owner != Me)
			{
				continue;
			}
			if (bFirst)
			{
				S.Card.bShow = true;
				S.Card.Name = D.Name;
				S.Card.Portrait = Card(D.Key);
				S.Card.Health = U.Hp;
				S.Card.MaxHealth = U.MaxHp;
			}
			if (Main.Num() < 2 * S.MainColumns)
			{
				FRTSHudSlot X = Slot(FName(*FString::Printf(TEXT("unit_%u"), Id)), Btn(D.Key), D.Name, FString());
				X.Health = float(U.Hp) / float(FMath::Max(1, U.MaxHp));
				X.bCaptain = bFirst;
				Main.Add(X);
			}
			bFirst = false;
		}
		if (!Main.IsEmpty())
		{
			S.bCommandPanel = true;
			S.Actions[0] = Action(TEXT("cmd_attack_move"), TEXT('A'), LOCTEXT("AttackMove", "Attack Move").ToString(), TEXT("icon:attack_move"));
			S.Actions[1] = Action(TEXT("cmd_stop"), TEXT('S'), LOCTEXT("Stop", "Stop").ToString(), TEXT("icon:stop"));
		}
	}
	if (!S.Card.bShow)
	{
		// nada selecionado: a placa da facção
		int32 Army = 0;
		int32 Buildings = 0;
		TArray<FRTSUnitState> Units;
		Match->GetUnits(Units);
		for (const FRTSUnitState& U : Units)
		{
			if (U.Owner == Me && U.Hp > 0)
			{
				(U.bStructure ? Buildings : Army) += 1;
			}
		}
		S.Card.bShow = true;
		S.Card.bFactionPlate = true;
		S.Card.FactionName = TEXT("UNITED STATES");
		S.Card.FactionEmblem = TEXT("emblem:usa");
		S.Card.PlateLine = FText::Format(LOCTEXT("PlateLine", "Army {0} · Buildings {1}"), Army, Buildings).ToString();
	}
	// as teclas do clássico, em ordem nas casas que têm tecla (QWERTYU na de cima, ZXCVBNM na de baixo)
	Main.SetNum(2 * S.MainColumns);
	const TCHAR* Row1 = TEXT("QWERTYUIO");
	const TCHAR* Row2 = TEXT("ZXCVBNM,.");
	NewHotkeys.Reset();
	for (int32 I = 0; I < Main.Num(); ++I)
	{
		if (Main[I].Key != 0)
		{
			Main[I].Key = (I < S.MainColumns ? Row1 : Row2)[I % S.MainColumns];
			NewHotkeys.Add(Main[I].Key, Main[I].Id);
		}
	}
	S.Main = MoveTemp(Main);

	// a dica: a casa sob o mouse no último quadro; o botão acende (HoverId) e afunda com o esquerdo segurado (PressedId)
	const FVector2D Mouse = MousePos();
	for (const FRTSHudButton& B : NewButtons)
	{
		if (!B.Box.IsInside(Mouse) || B.Id.ToString().StartsWith(TEXT("hud_bg")))
		{
			continue;
		}
		S.HoverId = B.Id;
		if (PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			S.PressedId = B.Id;
		}
		for (const FRTSHudSlot& X : S.Main)
		{
			if (!X.bEmpty && X.Id == B.Id && !X.Label.IsEmpty())
			{
				S.TooltipFor = X.Id;
				S.TooltipTitle = X.Label;
				S.TooltipKey = X.Key ? FString::Chr(X.Key) : FString();
				S.TooltipCost = X.Price;
				if (const FTip* T = Tips.Find(X.Id))
				{
					S.TooltipCost = T->Cost;
					S.TooltipLine = T->Line;
					S.bTooltipLineRed = T->bRed;
				}
				if (!X.bLocked && X.bNoMoney)
				{
					S.TooltipLine = LOCTEXT("NoMoneyTip", "Not enough money").ToString();
					S.bTooltipLineRed = true;
				}
			}
		}
		for (const FRTSHudSlot& X : S.Actions)
		{
			if (!X.bEmpty && X.Id == B.Id)
			{
				S.TooltipFor = X.Id;
				S.TooltipTitle = X.Label;
				S.TooltipKey = X.Key ? FString::Chr(X.Key) : FString();
			}
		}
		break;
	}
}

void ARTSHUD::DrawNewHud(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	if (!NewPainter)
	{
		const FString Repo = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".."));
		NewKit = MakeShared<FRTSHudKit>(Repo / TEXT("docs/concept/hud/kit"), Repo / TEXT("HUDLab/LabAssets"));
		TWeakPtr<FRTSHudKit> WeakKit = NewKit;
		NewPainter = MakeShared<FRTSHudPainter>([WeakKit](FName N) -> UTexture*
		{
			const TSharedPtr<FRTSHudKit> K = WeakKit.Pin();
			return K && !N.IsNone() ? K->Get(N) : nullptr;
		}, Repo / TEXT("HUDLab/Fonts"));
	}
	FRTSHudState State;
	FillNewHud(PC, Match, State);
	// o minimapa do jogo por baixo, no losango da peça nova
	bNewMinimapGeometry = true;
	DrawMinimap(PC, Match);
	bNewMinimapGeometry = false;
	NewPainter->Draw(Canvas, State, NewButtons);
	for (const FRTSHudButton& B : NewButtons)
	{
		Buttons.Add({ B.Id, B.Box });
	}
	// as peças do HUD seguram o clique (clicar no painel nunca vira ordem no chão): o Draw devolve um "hud_bg"
	// por peça no fim da lista, depois dos botões de verdade (o ButtonAt pega o primeiro que contém o mouse);
	// o losango do minimapa é do gesto do minimapa, que o controle confere antes dos botões
}

#undef LOCTEXT_NAMESPACE
