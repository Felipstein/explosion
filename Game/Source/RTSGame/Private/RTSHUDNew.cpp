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

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "RTSHudKit.h"
#include "RTSHudPainter.h"
#include "RTSMapBaker.h"
#include "RTSMatchSubsystem.h"
#include "RTSPlayerController.h"

#define LOCTEXT_NAMESPACE "RTSHUD"

namespace
{
	// a foto do botão e o retrato do cartão (docs/concept/hud/kit/photos) pelo key do Units.json
	FString PhotoName(const FString& Key)
	{
		// (14/10) a Patriot usa o botão da defesa de mísseis do kit da Arte (a mesma bateria da USA) até ter foto própria;
		// (v4) o Tomahawk, o do lançador de foguetes (a foto existe; o modelo é o chassi do caminhão)
		if (Key == TEXT("usa_tomahawk"))
		{
			return TEXT("usa_rocket_launcher");
		}
		return Key == TEXT("usa_mbt") ? FString(TEXT("usa_battle_tank")) : (Key == TEXT("usa_patriot") ? FString(TEXT("usa_missile_defense")) : Key);
	}

	// (v4) as cores do plugin (RTSHudPainter.cpp, RTSHudColors), em sRGB como na planta; nomes próprios (o unity build junta
	// os .cpp do HUD)
	const FLinearColor V4Fill(FColor(13, 16, 21, 240));
	const FLinearColor V4Edge(FColor(84, 94, 108));
	const FLinearColor V4Blue(FColor(77, 143, 224));
	const FLinearColor V4Red(FColor(224, 68, 58));
	const FLinearColor V4Amber(FColor(240, 160, 32));
	const FLinearColor V4Green(FColor(57, 196, 90));
	const FLinearColor V4GoldText(FColor(242, 209, 107));
	const FLinearColor V4TextMain(FColor(233, 238, 244));
	const FLinearColor V4TextSub(FColor(154, 166, 180));
	const FLinearColor V4Key(FColor(255, 215, 106));

	/** "2:31": os segundos que faltam, arredondados pra cima (o relógio não mostra 0:00 antes de acabar). */
	FString V4Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}

	FLinearColor V4Alpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * A);
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
	// (v4, 23/10) a patente do general: as estrelas e a barra do XP dentro da patente (cheia na 5); sem o Promote (os pontos
	// pra gastar ficam pra depois)
	const int32 MyRank = Match->PlayerRank(Me);
	S.Stars = MyRank;
	{
		const int32 From = Match->RankXpThreshold(MyRank);
		const int32 To = Match->RankXpThreshold(MyRank + 1);
		S.Xp = MyRank >= Sim::MaxRank || To <= From ? 1.0f : FMath::Clamp(float(Match->PlayerXp(Me) - From) / float(To - From), 0.0f, 1.0f);
	}
	S.Promote = 0;
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
	// (v4) os avisos que nascem dos eventos (URTSMatchSubsystem::HudNotices): o ataque de mísseis chegando perto do que é seu
	// (vermelho, em cima de todos, 5 s), a patente nova (dourado, 6 s) e o avião de carga a caminho (âmbar, 5 s). Do mesmo
	// tipo, só o mais novo. O ataque fica sem a seta da borda: a do plugin é fixa, na direita apontando pra direita (não
	// recebe direção) e mandaria o jogador pro lado errado; o lugar vem do pisca no minimapa e do Espaço (o controle)
	const URTSMatchSubsystem::FHudNotice* Strike = nullptr;
	const URTSMatchSubsystem::FHudNotice* Promoted = nullptr;
	const URTSMatchSubsystem::FHudNotice* Airlift = nullptr;
	for (const URTSMatchSubsystem::FHudNotice& N : Match->HudNotices())
	{
		const float Age = float(Now - N.At);
		switch (N.Kind)
		{
		case URTSMatchSubsystem::FHudNotice::EKind::StrikeIncoming: Strike = Age < 5.0f ? &N : Strike; break;
		case URTSMatchSubsystem::FHudNotice::EKind::Promoted: Promoted = Age < 6.0f ? &N : Promoted; break;
		case URTSMatchSubsystem::FHudNotice::EKind::AirliftInbound: Airlift = Age < 5.0f ? &N : Airlift; break;
		default: break;
		}
	}
	if (Strike)
	{
		FRTSHudAlert& A = S.Alerts.Add_GetRef({ LOCTEXT("AlertStrikeIncoming", "Missile strike incoming!").ToString(), FLinearColor(FColor(224, 68, 58)) });
		A.Age = float(Now - Strike->At);
	}
	if (PC->AttackMessageAlpha() > 0.0f)
	{
		FRTSHudAlert& A = S.Alerts.Add_GetRef({ LOCTEXT("AlertUnderAttack", "Unit under attack").ToString(), FLinearColor(FColor(224, 68, 58)) });
		A.Age = PC->AttackMessageSeconds();
	}
	if (Promoted)
	{
		// a linha cabe na caixa do aviso; o que a patente liberou vai inteiro no letreiro do meio (DrawDefeats)
		FRTSHudAlert& A = S.Alerts.Add_GetRef({ FText::Format(LOCTEXT("AlertPromoted", "PROMOTED - RANK {0}"), Promoted->Value).ToString(), V4GoldText });
		A.Age = float(Now - Promoted->At);
	}
	if (Airlift)
	{
		FRTSHudAlert& A = S.Alerts.Add_GetRef({ LOCTEXT("AlertAirlift", "Airlift inbound").ToString(), V4Amber });
		A.Age = float(Now - Airlift->At);
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

	// (v4) a coluna de poderes: um botão por poder que a patente já ligou (de baixo pra cima, a ordem das teclas F1–F3; antes
	// da patente 2 a coluna nem aparece), o ícone dos dados, READY respirando ou o tempo caindo; sem o Command Center fica
	// apagado com OFF (a dica diz o que falta)
	NewHoverPowerSlot = -1;
	for (int32 PowerSlot = 0; PowerSlot < 4; ++PowerSlot)
	{
		const int32 Power = Match->PowerAtSlot(PowerSlot);
		URTSMatchSubsystem::FPowerInfo P;
		if (Power < 0 || !Match->GetPowerInfo(Power, P))
		{
			break;
		}
		FRTSHudPower H;
		H.Icon = P.Icon;
		if (!P.bGranter)
		{
			H.Remaining = 1.0f;
			H.Time = LOCTEXT("PowerOff", "OFF").ToString();
		}
		else
		{
			// (o plugin pinta READY com Remaining 0: esperando, nunca exatamente 0)
			H.Remaining = P.ReadyInS > 0.0f ? FMath::Max(P.Remaining, 0.001f) : 0.0f;
			H.Time = V4Clock(P.ReadyInS);
		}
		S.Powers.Add(H);
	}

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
	// "Requires: War Factory": o que falta pro tipo (a dica aprovada: "o que falta, em vermelho"). (v4) Só os prédios prontos
	// que o jogador ainda não tem, e a patente ("Requires: Rank 3"); sem achar o que falta, todos os que o tipo pede
	TArray<FRTSUnitState> MyUnits;
	Match->GetUnits(MyUnits);
	MyUnits.RemoveAll([Me](const FRTSUnitState& U) { return U.Owner != Me || U.Hp <= 0; });
	const auto Requires = [Match, MyRank, &MyUnits](const FRTSDefInfo& D)
	{
		TArray<FString> Names;
		for (const uint16 R : D.Requires)
		{
			FRTSDefInfo RD;
			const bool bHave = MyUnits.ContainsByPredicate([R](const FRTSUnitState& U) { return U.Def == R && U.bStructure && U.BuildFraction >= 1.0f; });
			if (!bHave && Match->GetDef(R, RD))
			{
				Names.Add(RD.Name);
			}
		}
		if (D.RequiresRank > MyRank)
		{
			Names.Add(FText::Format(LOCTEXT("TipRank", "Rank {0}"), D.RequiresRank).ToString());
		}
		for (int32 K = 0; Names.IsEmpty() && K < D.Requires.Num(); ++K)
		{
			FRTSDefInfo RD;
			if (Match->GetDef(D.Requires[K], RD))
			{
				Names.Add(RD.Name);
			}
		}
		return FText::Format(LOCTEXT("TipRequires", "Requires: {0}"), FText::FromString(FString::Join(Names, TEXT(", ")))).ToString();
	};
	// (v4) o teto por jogador (a zona de lançamento: 2, as obras contam): o botão fica com o cadeado e a dica diz o teto
	const auto AtLimit = [&MyUnits](uint16 Def, const FRTSDefInfo& D)
	{
		return D.MaxPerPlayer > 0 && MyUnits.FilterByPredicate([Def](const FRTSUnitState& U) { return U.Def == Def; }).Num() >= D.MaxPerPlayer;
	};
	const auto LimitTip = [](const FRTSDefInfo& D)
	{
		return FText::Format(LOCTEXT("TipLimit", "Limit reached ({0})"), D.MaxPerPlayer).ToString();
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
					const bool bMissing = !Match->HasPrerequisites(P);
					const bool bFull = !bMissing && AtLimit(P, PD);
					X.bLocked = bMissing || bFull;
					X.bNoMoney = Cash < PD.Cost;
					Tips.Add(X.Id, bMissing ? FTip{ CostAndTime(PD.Cost, PD.BuildSeconds), Requires(PD), true }
						: (bFull ? FTip{ CostAndTime(PD.Cost, PD.BuildSeconds), LimitTip(PD), true }
						: FTip{ CostAndTime(PD.Cost, PD.BuildSeconds), LOCTEXT("TipProduce", "Left-click +1 · Shift-click +5 · Right-click −1").ToString(), false }));
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
				// (v4, 23/10) a zona de lançamento pronta, sozinha na seleção: o relógio do avião no lugar da grade
				// (FRTSHudPainter::DrawTimer, a imagem 4 do GPT que o Felipe escolheu; o CB_CONTEXT_OCL_TIMER do ZH). O segundo
				// mostrado é pra cima (nunca 0:00 antes do pouso); a barra anda em degraus de 1 s como no ZH (o texto e a barra só
				// mudam quando o segundo muda) e enche no último. Os paletes e o valor vêm da simulação (a rota: 3, 4, 5; 4 caixas
				// de $75 por palete). Sem a casa do avião na fila nem o texto ao lado do cartão: o relógio é só este. Sendo vendida,
				// sem relógio (a simulação para a contagem e o "sem energia" mentiria)
				URTSMatchSubsystem::FAirliftInfo Air;
				if (Selected.Num() == 1 && !U.bSelling && Match->GetAirlift(StructureId, Air))
				{
					const float Period = FMath::Max(1.0f, Air.PeriodS);
					const float LeftS = Air.bStarted ? FMath::Clamp(Air.LeftS, 0.0f, Period) : Period;
					S.Timer.bShow = true;
					S.Timer.Medallion = TEXT("skin:timer_medallion_airlift");
					S.Timer.Time = V4Clock(LeftS);
					S.Timer.Fill = FMath::Clamp(1.0f - FMath::FloorToFloat(LeftS) / Period, 0.0f, 1.0f);
					S.Timer.bInbound = Air.bInbound;
					S.Timer.bPaused = Air.bPaused;
					S.Timer.Title = (Air.bInbound ? LOCTEXT("AirliftTimerInbound", "AIRLIFT INBOUND")
						: (Air.bPaused ? LOCTEXT("AirliftTimerPaused", "PAUSED · LOW POWER") : LOCTEXT("AirliftTimerNext", "NEXT AIRLIFT"))).ToString();
					S.Timer.Line = FText::Format(LOCTEXT("AirliftTimerLine", "{0} pallets  ·  +{1}"), Air.NextPallets, FText::FromString(Money(Air.NextValue))).ToString();
				}
				// (v4, 23/10) o ponto de encontro só em quem produz (a zona de lançamento, a usina e a Patriot não soltam unidade)
				if (!D.Produces.IsEmpty())
				{
					S.Actions[3] = Action(TEXT("cmd_rally"), 0, LOCTEXT("RallyPoint", "Rally Point").ToString(), TEXT("icon:rally_point"));
				}
				// (v4, 23/10) vender (o Sell do ZH): todo prédio pronto do jogador, na casa do Cancelar da obra; volta metade do custo
				if (Match->CanSell(StructureId))
				{
					S.Actions[5] = Action(TEXT("cmd_sell"), 0, FText::Format(LOCTEXT("SellAction", "Sell (+{0})"), FText::FromString(Money(Match->SellRefund(StructureId)))).ToString(),
						TEXT("icon:sell"));
				}
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
				const bool bMissing = !Match->HasPrerequisites(B);
				const bool bFull = !bMissing && AtLimit(B, BD);
				X.bLocked = bMissing || bFull;
				X.bNoMoney = Cash < BD.Cost;
				Tips.Add(X.Id, bMissing ? FTip{ CostAndTime(BD.Cost, BD.BuildSeconds), Requires(BD), true }
					: (bFull ? FTip{ CostAndTime(BD.Cost, BD.BuildSeconds), LimitTip(BD), true } : FTip{ CostAndTime(BD.Cost, BD.BuildSeconds), FString(), false }));
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
		if (B.Id.ToString().StartsWith(TEXT("power_")))
		{
			NewHoverPowerSlot = FCString::Atoi(*B.Id.ToString().Mid(6));   // (v4) a dica do poder sai do DrawNewHudExtras
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
	// (v4) o poder sendo mirado fica afundado na coluna (o "apertado" do plugin), até usar ou desistir
	for (int32 PowerSlot = 0; S.PressedId.IsNone() && PC->GetTargetingPower() >= 0 && PowerSlot < S.Powers.Num(); ++PowerSlot)
	{
		if (Match->PowerAtSlot(PowerSlot) == PC->GetTargetingPower())
		{
			S.PressedId = FName(*FString::Printf(TEXT("power_%d"), PowerSlot));
		}
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
		// (v4) a mesma letra do plugin pro que sai daqui (a dica do poder, o relógio do avião)
		NewFonts[0] = MakeShared<FStandaloneCompositeFont>(FName(TEXT("Regular")), Repo / TEXT("HUDLab/Fonts/BarlowSemiCondensed-SemiBold.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		NewFonts[1] = MakeShared<FStandaloneCompositeFont>(FName(TEXT("Regular")), Repo / TEXT("HUDLab/Fonts/BarlowSemiCondensed-Bold.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	}
	FRTSHudState State;
	FillNewHud(PC, Match, State);
	// o minimapa do jogo por baixo, no losango da peça nova
	bNewMinimapGeometry = true;
	DrawMinimap(PC, Match);
	bNewMinimapGeometry = false;
	NewPainter->Draw(Canvas, State, NewButtons);
	DrawNewHudExtras(PC, Match);
	for (const FRTSHudButton& B : NewButtons)
	{
		Buttons.Add({ B.Id, B.Box });
	}
	// as peças do HUD seguram o clique (clicar no painel nunca vira ordem no chão): o Draw devolve um "hud_bg"
	// por peça no fim da lista, depois dos botões de verdade (o ButtonAt pega o primeiro que contém o mouse);
	// o losango do minimapa é do gesto do minimapa, que o controle confere antes dos botões
}

// ---------------------------------------------------------------- (v4, 23/10) a patente, os poderes e a zona de lançamento

FVector2D ARTSHUD::NewMeasure(const FString& Text, float Px, bool bBold) const
{
	const TSharedPtr<const FCompositeFont>& Font = NewFonts[bBold ? 1 : 0];
	if (!Font || !Canvas || Text.IsEmpty() || !FSlateApplication::IsInitialized())
	{
		return FVector2D::ZeroVector;
	}
	// as contas do FRTSHudPainter::Measure: o tamanho em px da planta, 1 px = 0,75 pt, na escala da tela
	const float Dpi = Canvas->GetDPIScale();
	const FSlateFontInfo Info(Font, FMath::Max(4.0f, Px * NewS * 0.75f), FName(TEXT("Regular")));
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Info, Dpi) / Dpi;
}

FVector2D ARTSHUD::NewText(const FString& Text, float X, float Y, float Px, const FLinearColor& Color, bool bBold, bool bRight)
{
	const FVector2D E = NewMeasure(Text, Px, bBold);
	const TSharedPtr<const FCompositeFont>& Font = NewFonts[bBold ? 1 : 0];
	if (!Font || !Canvas || Text.IsEmpty())
	{
		return E;
	}
	FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(bRight ? X - float(E.X) : X), FMath::RoundToFloat(Y)), FText::FromString(Text),
		FSlateFontInfo(Font, FMath::Max(4.0f, Px * NewS * 0.75f), FName(TEXT("Regular"))), Color);
	// (o Canvas só desenha texto com um UFont de cache em tempo real; a letra de verdade é a da FSlateFontInfo, como no plugin)
	Item.Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f * Color.A), FVector2D(FMath::Max(1.0f, FMath::RoundToFloat(NewS))));
	Canvas->DrawItem(Item);
	return E;
}

FString ARTSHUD::PromotionText(URTSMatchSubsystem* Match, int32 Rank)
{
	// o que a patente liberou sai dos dados: os poderes que ligam ou crescem nela e as unidades que ela destrava
	const FText Unlocks = Match ? Match->UnlocksAtRank(Rank) : FText::GetEmpty();
	return Unlocks.IsEmpty() ? FText::Format(LOCTEXT("PromotedLineShort", "PROMOTED - RANK {0}"), Rank).ToString()
		: FText::Format(LOCTEXT("PromotedLine", "PROMOTED - RANK {0}: {1}"), Rank, Unlocks).ToString();
}

void ARTSHUD::DrawNewHudExtras(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	FRTSHudPainter::Layout(Canvas->ClipX, Canvas->ClipY, NewS, NewLeft, NewWidth);
	const float S = NewS;
	const float Px1 = FMath::Max(1.0f, S);

	// a dica do poder sob o mouse: ao lado do botão (a coluna fica na borda esquerda, em cima do minimapa), no desenho da dica
	// do plugin: o nome, a tecla em amarelo, o estado (verde pronto, vermelho o que falta) e os números
	const int32 Power = NewHoverPowerSlot >= 0 ? Match->PowerAtSlot(NewHoverPowerSlot) : -1;
	const FName HoverId(*FString::Printf(TEXT("power_%d"), NewHoverPowerSlot));
	const FRTSHudButton* HoverButton = NewButtons.FindByPredicate([&HoverId](const FRTSHudButton& B) { return B.Id == HoverId; });
	URTSMatchSubsystem::FPowerInfo P;
	if (Power >= 0 && HoverButton && Match->GetPowerInfo(Power, P))
	{
		const FString Key = NewHoverPowerSlot < 3 ? FString::Printf(TEXT("[F%d]"), NewHoverPowerSlot + 1) : FString();
		const FText Why = Match->PowerRefusal(Power);
		const FString Line = Why.IsEmpty() ? LOCTEXT("PowerTipReady", "Ready: click it, then click the target").ToString() : Why.ToString();
		const FLinearColor LineColor = Why.IsEmpty() ? V4Green : (P.bGranter ? V4TextSub : V4Red);
		FString Stats = FText::Format(LOCTEXT("PowerTipStats", "{0} missiles in {1} m  ·  recharge {2}"), P.Missiles, FMath::RoundToInt(P.RadiusM),
			FText::FromString(V4Clock(P.CooldownS))).ToString();
		if (!P.bUpgraded && P.UpgradeRank > 0)
		{
			Stats += FText::Format(LOCTEXT("PowerTipGrows", "  ·  grows at Rank {0}"), P.UpgradeRank).ToString();
		}
		const float Pad = 14.0f * S;
		const FVector2D ET = NewMeasure(P.Name, 17.0f, true);
		const FVector2D EK = NewMeasure(Key, 17.0f, true);
		const FVector2D EL = NewMeasure(Line, 15.0f, false);
		const FVector2D ES = NewMeasure(Stats, 14.0f, false);
		const float BW = FMath::Max3(float(ET.X) + 6.0f * S + float(EK.X), float(EL.X), float(ES.X)) + 2.0f * Pad;
		const float BH = 80.0f * S;
		const float BX = FMath::Min(float(HoverButton->Box.Max.X) + 10.0f * S, Canvas->ClipX - BW - 8.0f * S);
		const float BY = FMath::Clamp(float(HoverButton->Box.GetCenter().Y) - BH * 0.5f, 8.0f * S, Canvas->ClipY - BH - 8.0f * S);
		DrawRect(V4Fill, BX, BY, BW, BH);
		DrawRect(V4Edge, BX, BY, BW, Px1);
		DrawRect(V4Edge, BX, BY + BH - Px1, BW, Px1);
		DrawRect(V4Edge, BX, BY, Px1, BH);
		DrawRect(V4Edge, BX + BW - Px1, BY, Px1, BH);
		NewText(P.Name, BX + Pad, BY + 8.0f * S, 17.0f, V4TextMain, true);
		NewText(Key, BX + Pad + float(ET.X) + 6.0f * S, BY + 8.0f * S, 17.0f, V4Key, true);
		NewText(Line, BX + Pad, BY + 33.0f * S, 15.0f, LineColor, false);
		NewText(Stats, BX + Pad, BY + 54.0f * S, 14.0f, V4TextSub, false);
	}
	// (o relógio do avião saiu daqui, 23/10: é o DrawTimer do plugin, no painel do meio com a zona selecionada)
	(void)PC;
}

void ARTSHUD::DrawPowerTarget(ARTSPlayerController* PC, URTSMatchSubsystem* Match)
{
	const int32 Power = PC->GetTargetingPower();
	const FRTSBakedMap* Map = Match->BakedMap();
	URTSMatchSubsystem::FPowerInfo P;
	FVector Ground;
	if (Power < 0 || !Map || !Match->GetPowerInfo(Power, P) || !PC->GetCursorGround(Ground))
	{
		return;
	}
	// o círculo do raio no chão (48 pedaços seguindo o relevo): azul onde vale, vermelho onde a simulação recusaria, com o
	// motivo no mouse ("Unexplored"). O forte é o girassol (onde os mísseis miram); o fraco, até onde o estilhaço ainda dói
	const FVector2D C(Ground.X, Ground.Y);
	const FText Why = Match->PowerRefusal(Power, &C);
	const bool bOk = Why.IsEmpty();
	const FLinearColor Color = bOk ? V4Blue : V4Red;
	const auto Ring = [this, PC, Map, &C](double RadiusCm, const FLinearColor& RingColor, float Thickness)
	{
		constexpr int32 Segments = 48;
		FVector2D Prev = FVector2D::ZeroVector;
		bool bPrev = false;
		for (int32 K = 0; K <= Segments; ++K)
		{
			const double A = 2.0 * UE_DOUBLE_PI * double(K) / double(Segments);
			const FVector2D Pt = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * RadiusCm;
			FVector2D Screen;
			const bool bOn = PC->ProjectWorldLocationToScreen(FVector(Pt.X, Pt.Y, Map->GroundZCm(Pt) + 30.0), Screen);
			if (bOn && bPrev)
			{
				DrawLine(Prev.X, Prev.Y, Screen.X, Screen.Y, RingColor, Thickness);
			}
			Prev = Screen;
			bPrev = bOn;
		}
	};
	Ring(P.RadiusM * 100.0, Color, 2.5f);
	if (P.SplashM > 0.0f)
	{
		Ring((P.RadiusM + P.SplashM) * 100.0, V4Alpha(Color, 0.4f), 1.0f);
	}
	FVector2D Mid;
	if (PC->ProjectWorldLocationToScreen(FVector(C.X, C.Y, Map->GroundZCm(C) + 30.0), Mid))
	{
		DrawLine(Mid.X - 8.0f, Mid.Y, Mid.X + 8.0f, Mid.Y, Color, 2.0f);
		DrawLine(Mid.X, Mid.Y - 8.0f, Mid.X, Mid.Y + 8.0f, Color, 2.0f);
	}
	const FVector2D M = MousePos();
	DrawLabel(FText::Format(LOCTEXT("PowerAimTitle", "{0}  ·  {1} missiles"), FText::FromString(P.Name), P.Missiles).ToString(), M.X + 22.0f, M.Y + 14.0f, 14, V4TextMain, false, true);
	if (bOk)
	{
		DrawLabel(LOCTEXT("PowerAimHint", "Left-click: fire  ·  Right-click: cancel").ToString(), M.X + 22.0f, M.Y + 36.0f, 12, V4TextSub);
	}
	else
	{
		DrawLabel(Why.ToString(), M.X + 22.0f, M.Y + 36.0f, 14, V4Red, false, true);
	}
}

void ARTSHUD::DrawPowerMessage(ARTSPlayerController* PC)
{
	// a recusa do botão ou da tecla (sem estar pronto, sem o Command Center): 2 s no mouse, a última meia desbotando; mirando,
	// o motivo já aparece embaixo do círculo
	float Age = 99.0f;
	const FText& Message = PC->GetPowerMessage(Age);
	if (Message.IsEmpty() || Age > 2.0f || PC->GetTargetingPower() >= 0)
	{
		return;
	}
	const FVector2D M = MousePos();
	DrawLabel(Message.ToString(), M.X + 22.0f, M.Y + 14.0f, 14, V4Alpha(V4Red, FMath::Clamp((2.0f - Age) / 0.5f, 0.0f, 1.0f)), false, true);
}

#undef LOCTEXT_NAMESPACE
